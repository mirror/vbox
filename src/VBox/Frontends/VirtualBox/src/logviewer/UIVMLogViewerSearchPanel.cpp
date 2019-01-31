/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class implementation.
 */

/*
 * Copyright (C) 2010-2019 Oracle Corporation
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
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#if defined(RT_OS_SOLARIS)
# include <QFontDatabase>
#endif
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextBlock>

/* GUI includes: */
#include "QIToolButton.h"
#include "UIIconPool.h"
#include "UIVMLogPage.h"
#include "UIVMLogViewerSearchPanel.h"
#include "UIVMLogViewerWidget.h"
#ifdef VBOX_WS_MAC
# include "VBoxUtils-darwin.h"
#endif

class UIVMLogViewerSearchField: public QLineEdit
{
    Q_OBJECT;

public:

    UIVMLogViewerSearchField(QWidget *pParent)
        : QLineEdit(pParent)
    {
        m_baseBrush = palette().base();
    }

    void markError()
    {
        QPalette pal = palette();
        QColor c(Qt::red);
        c.setAlphaF(0.3);
        pal.setBrush(QPalette::Base, c);
        setPalette(pal);
    }

    void unmarkError()
    {
        QPalette pal = palette();
        pal.setBrush(QPalette::Base, m_baseBrush);
        setPalette(pal);
    }

private:
    /* Private member vars */
    QBrush m_baseBrush;
};

UIVMLogViewerSearchPanel::UIVMLogViewerSearchPanel(QWidget *pParent, UIVMLogViewerWidget *pViewer)
    : UIVMLogViewerPanel(pParent, pViewer)
    , m_pSearchEditor(0)
    , m_pNextButton(0)
    , m_pPreviousButton(0)
    , m_pCaseSensitiveCheckBox(0)
    , m_pMatchWholeWordCheckBox(0)
    , m_pHighlightAllCheckBox(0)
    , m_iSearchPosition(0)
    , m_iMatchCount(0)
{
    /* Prepare: */
    prepare();
}

void UIVMLogViewerSearchPanel::refresh()
{
    m_iSearchPosition = 0;
    /* We start the search from the end of the doc. assuming log's end is more interesting: */
    search(BackwardSearch, true);
    emit sigHighlightingUpdated();
}

void UIVMLogViewerSearchPanel::reset()
{
    m_iSearchPosition = 0;
    m_matchLocationVector.clear();
    m_iMatchCount = 0;
}

const QVector<float> &UIVMLogViewerSearchPanel::matchLocationVector() const
{
    return m_matchLocationVector;
}

QString UIVMLogViewerSearchPanel::panelName() const
{
    return "SearchPanel";
}

int UIVMLogViewerSearchPanel::marchCount() const
{
    return m_iMatchCount;
}

void UIVMLogViewerSearchPanel::hideEvent(QHideEvent *pEvent)
{
    /* Get focus-widget: */
    QWidget *pFocus = QApplication::focusWidget();
    /* If focus-widget is valid and child-widget of search-panel,
     * focus next child-widget in line: */
    if (pFocus && pFocus->parent() == this)
        focusNextPrevChild(true);
    /* Call to base-class: */
    UIVMLogViewerPanel::hideEvent(pEvent);
    reset();
}

void UIVMLogViewerSearchPanel::sltSearchTextChanged(const QString &strSearchString)
{
    /* Enable/disable Next-Previous buttons as per search-string validity: */
    m_pNextButton->setEnabled(!strSearchString.isEmpty());
    m_pPreviousButton->setEnabled(!strSearchString.isEmpty());


    /* If search-string is not empty: */
    if (!strSearchString.isEmpty())
    {
        /* Reset the position to force the search restart from the document's end: */
        m_iSearchPosition = 0;
        search(BackwardSearch, true);
        emit sigHighlightingUpdated();
        return;
    }
    /* If search-string is empty, reset cursor position: */
    if (!viewer())
        return;

    QPlainTextEdit *pBrowser = textEdit();
    if (!pBrowser)
        return;
    /* If  cursor has selection: */
    if (pBrowser->textCursor().hasSelection())
    {
        /* Get cursor and reset position: */
        QTextCursor cursor = pBrowser->textCursor();
        cursor.setPosition(cursor.anchor());
        pBrowser->setTextCursor(cursor);
    }
    m_iSearchPosition = -1;
    m_iMatchCount = 0;
    emit sigSearchUpdated();
    clearHighlighting();
}

void UIVMLogViewerSearchPanel::sltHighlightAllCheckBox()
{
    if (!viewer())
        return;

    QTextDocument *pDocument = textDocument();
    if (!pDocument)
        return;

    if (m_pHighlightAllCheckBox->isChecked())
    {
        const QString &searchString = m_pSearchEditor->text();
        if (searchString.isEmpty())
            return;
        highlightAll(pDocument, searchString);
    }
    else
        clearHighlighting();

    emit sigHighlightingUpdated();
}

void UIVMLogViewerSearchPanel::sltCaseSentitiveCheckBox()
{
    refresh();
}

void UIVMLogViewerSearchPanel::sltMatchWholeWordCheckBox()
{
    refresh();
}

void UIVMLogViewerSearchPanel::prepareWidgets()
{
    if (!mainLayout())
        return;

    /* Create search field layout: */
    QHBoxLayout *pSearchFieldLayout = new QHBoxLayout;
    if (pSearchFieldLayout)
    {
        pSearchFieldLayout->setContentsMargins(0, 0, 0, 0);
#ifdef VBOX_WS_MAC
        pSearchFieldLayout->setSpacing(5);
#else
        pSearchFieldLayout->setSpacing(qApp->style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing) / 2);
#endif

        /* Create search-editor: */
        m_pSearchEditor = new UIVMLogViewerSearchField(0 /* parent */);
        if (m_pSearchEditor)
        {
            m_pSearchEditor->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            pSearchFieldLayout->addWidget(m_pSearchEditor);
        }

        /* Create search button layout: */
        QHBoxLayout *pSearchButtonsLayout = new QHBoxLayout;
        if (pSearchButtonsLayout)
        {
            pSearchButtonsLayout->setContentsMargins(0, 0, 0, 0);
            pSearchButtonsLayout->setSpacing(0);

            /* Create Previous button: */
            m_pPreviousButton = new QIToolButton;
            if (m_pPreviousButton)
            {
                m_pPreviousButton->setIcon(UIIconPool::iconSet(":/log_viewer_search_backward_16px.png"));
                pSearchButtonsLayout->addWidget(m_pPreviousButton);
            }

            /* Create Next button: */
            m_pNextButton = new QIToolButton;
            if (m_pNextButton)
            {
                m_pNextButton->setIcon(UIIconPool::iconSet(":/log_viewer_search_forward_16px.png"));
                pSearchButtonsLayout->addWidget(m_pNextButton);
            }

            pSearchFieldLayout->addLayout(pSearchButtonsLayout);
        }

        mainLayout()->addLayout(pSearchFieldLayout);
    }

    /* Create case-sensitive check-box: */
    m_pCaseSensitiveCheckBox = new QCheckBox;
    if (m_pCaseSensitiveCheckBox)
    {
        mainLayout()->addWidget(m_pCaseSensitiveCheckBox);
    }

    /* Create whole-word check-box: */
    m_pMatchWholeWordCheckBox = new QCheckBox;
    if (m_pMatchWholeWordCheckBox)
    {
        setFocusProxy(m_pMatchWholeWordCheckBox);
        mainLayout()->addWidget(m_pMatchWholeWordCheckBox);
    }

    /* Create highlight-all check-box: */
    m_pHighlightAllCheckBox = new QCheckBox;
    if (m_pHighlightAllCheckBox)
    {
        mainLayout()->addWidget(m_pHighlightAllCheckBox);
    }
}

void UIVMLogViewerSearchPanel::prepareConnections()
{
    connect(m_pSearchEditor, &UIVMLogViewerSearchField::textChanged, this, &UIVMLogViewerSearchPanel::sltSearchTextChanged);
    connect(m_pNextButton, &QIToolButton::clicked, this, &UIVMLogViewerSearchPanel::findNext);
    connect(m_pPreviousButton, &QIToolButton::clicked, this, &UIVMLogViewerSearchPanel::findPrevious);

    connect(m_pHighlightAllCheckBox, &QCheckBox::stateChanged,
            this, &UIVMLogViewerSearchPanel::sltHighlightAllCheckBox);
    connect(m_pCaseSensitiveCheckBox, &QCheckBox::stateChanged,
            this, &UIVMLogViewerSearchPanel::sltCaseSentitiveCheckBox);
    connect(m_pMatchWholeWordCheckBox, &QCheckBox::stateChanged,
            this, &UIVMLogViewerSearchPanel::sltMatchWholeWordCheckBox);
}

void UIVMLogViewerSearchPanel::retranslateUi()
{
    UIVMLogViewerPanel::retranslateUi();

    m_pSearchEditor->setToolTip(UIVMLogViewerWidget::tr("Enter a search string here"));
    m_pNextButton->setToolTip(UIVMLogViewerWidget::tr("Search for the next occurrence of the string (F3)"));
    m_pPreviousButton->setToolTip(UIVMLogViewerWidget::tr("Search for the previous occurrence of the string (Shift+F3)"));

    m_pCaseSensitiveCheckBox->setText(UIVMLogViewerWidget::tr("C&ase Sensitive"));
    m_pCaseSensitiveCheckBox->setToolTip(UIVMLogViewerWidget::tr("When checked, perform case sensitive search"));

    m_pMatchWholeWordCheckBox->setText(UIVMLogViewerWidget::tr("Ma&tch Whole Word"));
    m_pMatchWholeWordCheckBox->setToolTip(UIVMLogViewerWidget::tr("When checked, search matches only complete words"));

    m_pHighlightAllCheckBox->setText(UIVMLogViewerWidget::tr("&Highlight All"));
    m_pHighlightAllCheckBox->setToolTip(UIVMLogViewerWidget::tr("When checked, all occurence of the search text are highlighted"));
}

void UIVMLogViewerSearchPanel::keyPressEvent(QKeyEvent *pEvent)
{
    switch (pEvent->key())
    {
        /* Process Enter press as 'search-next',
         * performed for any search panel widget: */
        case Qt::Key_Enter:
        case Qt::Key_Return:
            {
                if (pEvent->modifiers() == 0 ||
                    pEvent->modifiers() & Qt::KeypadModifier)
                {
                    /* Animate click on 'Next' button: */
                m_pNextButton->animateClick();
                return;
                }
                break;
            }
        default:
            break;
    }
    /* Call to base-class: */
    UIVMLogViewerPanel::keyPressEvent(pEvent);
}

bool UIVMLogViewerSearchPanel::eventFilter(QObject *pObject, QEvent *pEvent)
{
    /* Depending on event-type: */
    switch (pEvent->type())
    {
        /* Process key press only: */
        case QEvent::KeyPress:
        {
            /* Cast to corresponding key press event: */
            QKeyEvent *pKeyEvent = static_cast<QKeyEvent*>(pEvent);

            /* Handle F3/Shift+F3 as search next/previous shortcuts: */
            if (pKeyEvent->key() == Qt::Key_F3)
            {
                /* If there is no modifier 'Key-F3' is pressed: */
                if (pKeyEvent->QInputEvent::modifiers() == 0)
                {
                    /* Animate click on 'Next' button: */
                    m_pNextButton->animateClick();
                    return true;
                }
                /* If there is 'ShiftModifier' 'Shift + Key-F3' is pressed: */
                else if (pKeyEvent->QInputEvent::modifiers() == Qt::ShiftModifier)
                {
                    /* Animate click on 'Prev' button: */
                    m_pPreviousButton->animateClick();
                    return true;
                }
            }
            /* Handle Ctrl+F key combination as a shortcut to focus search field: */
            else if (pKeyEvent->QInputEvent::modifiers() == Qt::ControlModifier &&
                     pKeyEvent->key() == Qt::Key_F)
            {
                /* Make sure current log-page is visible: */
                if (isHidden())
                    show();
                /* Set focus on search-editor: */
                m_pSearchEditor->setFocus();
                return true;
            }
            /* Handle alpha-numeric keys to implement the "find as you type" feature: */
            else if ((pKeyEvent->QInputEvent::modifiers() & ~Qt::ShiftModifier) == 0 &&
                     pKeyEvent->key() >= Qt::Key_Exclam && pKeyEvent->key() <= Qt::Key_AsciiTilde)
            {
                /* Make sure current log-page is visible: */
                if (isHidden())
                    show();
                /* Set focus on search-editor: */
                m_pSearchEditor->setFocus();
                /* Insert the text to search-editor, which triggers the search-operation for new text: */
                m_pSearchEditor->insert(pKeyEvent->text());
                return true;
            }
            break;
        }
    default:
        break;
    }
    /* Call to base-class: */
    return UIVMLogViewerPanel::eventFilter(pObject, pEvent);
}

void UIVMLogViewerSearchPanel::showEvent(QShowEvent *pEvent)
{
    /* Call to base-class: */
    UIVMLogViewerPanel::showEvent(pEvent);
    if (m_pSearchEditor)
    {
        /* Set focus on search-editor: */
        m_pSearchEditor->setFocus();
        /* Select all the text: */
        m_pSearchEditor->selectAll();
    }
}

void UIVMLogViewerSearchPanel::search(SearchDirection direction, bool highlight)
{
    QPlainTextEdit *pTextEdit = textEdit();
    if (!pTextEdit)
        return;
    QTextDocument *pDocument = textDocument();
    if (!pDocument)
        return;
    if (!m_pSearchEditor)
        return;

    const QString &searchString = m_pSearchEditor->text();
    m_iMatchCount = countMatches(pDocument, searchString);
    emit sigSearchUpdated();

    if (searchString.isEmpty())
        return;

    QTextCursor endCursor(pDocument);
    endCursor.movePosition(QTextCursor::End);
    QTextCursor startCursor(pDocument);

    if (m_pHighlightAllCheckBox->isChecked())
    {
        if (highlight)
            highlightAll(pDocument, searchString);
    }
    else
        m_matchLocationVector.clear();

    QTextCursor resultCursor(pDocument);
    int startPosition = m_iSearchPosition;
    if (direction == BackwardSearch)
        startPosition -= searchString.length();
    resultCursor = pDocument->find(searchString, startPosition, constructFindFlags(direction));

    /* Decide whether to wrap around or to end the search */
    if (resultCursor.isNull())
    {
        /* End the search if we search the whole document with no find: */
        if ((direction == ForwardSearch && startPosition == startCursor.position()) ||
            (direction == BackwardSearch && startPosition == endCursor.position()))
        {
            return;
        }
        /* Wrap the search */
        if (direction == ForwardSearch)
        {
            m_iSearchPosition = startCursor.position();
            search(ForwardSearch, false);
            return;
        }
        else
        {
            /* Set the search position away from the end position to be
               able to find the string at the end of the document: */
            m_iSearchPosition = endCursor.position() + searchString.length();
            search(BackwardSearch, false);
            return;
        }
    }
    pTextEdit->setTextCursor(resultCursor);
    m_iSearchPosition = resultCursor.position();
}

void UIVMLogViewerSearchPanel::findNext()
{
    search(ForwardSearch, false);
}

void UIVMLogViewerSearchPanel::findPrevious()
{
    search(BackwardSearch, false);
}

void UIVMLogViewerSearchPanel::clearHighlighting()
{
    if (!viewer())
        return;
    m_matchLocationVector.clear();
    QTextDocument* pDocument = textDocument();
    if (pDocument)
        pDocument->undo();

    emit sigHighlightingUpdated();
}

void UIVMLogViewerSearchPanel::highlightAll(QTextDocument *pDocument,
                                            const QString &searchString)
{
    clearHighlighting();
    if (!pDocument)
        return;
    if (searchString.isEmpty())
        return;

    QTextCursor highlightCursor(pDocument);
    QTextCharFormat colorFormat(highlightCursor.charFormat());
    QTextCursor cursor(pDocument);
    cursor.beginEditBlock();
    colorFormat.setBackground(Qt::yellow);
    int lineCount = pDocument->lineCount();
    QTextDocument::FindFlags flags = constructFindFlags(ForwardSearch);
    while (!highlightCursor.isNull() && !highlightCursor.atEnd())
    {
        /* Hightlighting searches is always from the top of the document forward: */
        highlightCursor = pDocument->find(searchString, highlightCursor, flags);
        if (!highlightCursor.isNull())
        {
            highlightCursor.mergeCharFormat(colorFormat);
            /* The following assumes we have single line blocks only: */
            int cursorLine = pDocument->findBlock(highlightCursor.position()).blockNumber();
            if (lineCount != 0)
                m_matchLocationVector.push_back(cursorLine / static_cast<float>(lineCount));
        }
    }
    cursor.endEditBlock();
}

int UIVMLogViewerSearchPanel::countMatches(QTextDocument *pDocument, const QString &searchString) const
{
    if (!pDocument)
        return 0;
    if (searchString.isEmpty())
        return 0;
    int count = 0;
    QTextCursor cursor(pDocument);
    QTextDocument::FindFlags flags = constructFindFlags(ForwardSearch);
    while (!cursor.isNull() && !cursor.atEnd())
    {
        cursor = pDocument->find(searchString, cursor, flags);

        if (!cursor.isNull())
            ++count;
    }
    return count;
}

QTextDocument::FindFlags UIVMLogViewerSearchPanel::constructFindFlags(SearchDirection eDirection) const
{
   QTextDocument::FindFlags findFlags;
   if (eDirection == BackwardSearch)
       findFlags = findFlags | QTextDocument::FindBackward;
   if (m_pCaseSensitiveCheckBox->isChecked())
       findFlags = findFlags | QTextDocument::FindCaseSensitively;
   if (m_pMatchWholeWordCheckBox->isChecked())
       findFlags = findFlags | QTextDocument::FindWholeWords;
   return findFlags;
}

#include "UIVMLogViewerSearchPanel.moc"
