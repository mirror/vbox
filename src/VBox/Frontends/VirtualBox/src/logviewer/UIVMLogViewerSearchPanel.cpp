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
# include <QAction>
# include <QCheckBox>
# include <QComboBox>
# if defined(RT_OS_SOLARIS)
#  include <QFontDatabase>
# endif
# include <QHBoxLayout>
# include <QLabel>
# include <QLineEdit>
# include <QPlainTextEdit>
# include <QTextBlock>

/* GUI includes: */
# include "QIToolButton.h"
# include "UIIconPool.h"
# include "UIVMLogPage.h"
# include "UIVMLogViewerSearchPanel.h"
# include "UIVMLogViewerWidget.h"
# ifdef VBOX_WS_MAC
#  include "VBoxUtils-darwin.h"
# endif

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

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
    , m_pSearchLabel(0)
    , m_pSearchEditor(0)
    , m_pNextButton(0)
    , m_pPreviousButton(0)
    , m_pNextPreviousButtonContainer(0)
    , m_pCaseSensitiveCheckBox(0)
    , m_pMatchWholeWordCheckBox(0)
    , m_pHighlightAllCheckBox(0)
    , m_pWarningIcon(0)
    , m_pInfoLabel(0)
    , m_iSearchPosition(0)
    , m_iMatchCount(-1)
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
    m_iMatchCount = -1;
    configureInfoLabels();
}

const QVector<float> &UIVMLogViewerSearchPanel::getMatchLocationVector() const
{
    return m_matchLocationVector;
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
    clearHighlighting(-1);
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
    {
        /* we need this check not to remove the 'not found' label
           when the user toggles with this checkbox: */
        if (m_iMatchCount != 0)
            clearHighlighting(-1);
        else
            clearHighlighting(0);
    }
    configureInfoLabels();
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

    /* Create search-editor: */
    m_pSearchEditor = new UIVMLogViewerSearchField(0 /* parent */);
    if (m_pSearchEditor)
    {
        /* Configure search-editor: */
        m_pSearchEditor->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        /* Add search-editor to main-layout: */
        mainLayout()->addWidget(m_pSearchEditor);
    }

    /* Create search-label: */
    m_pSearchLabel = new QLabel;
    if (m_pSearchLabel)
    {
        /* Configure search-label: */
        m_pSearchLabel->setBuddy(m_pSearchEditor);
        /* Prepare font: */
#ifdef VBOX_DARWIN_USE_NATIVE_CONTROLS
        QFont font = m_pSearchLabel->font();
        font.setPointSize(::darwinSmallFontSize());
        m_pSearchLabel->setFont(font);
#endif /* VBOX_DARWIN_USE_NATIVE_CONTROLS */
        /* Add search-label to main-layout: */
        mainLayout()->addWidget(m_pSearchLabel);
    }

    /* Create Next/Previous buttons: */
    m_pNextPreviousButtonContainer = new QFrame;
    if (m_pNextPreviousButtonContainer)
    {
        mainLayout()->addWidget(m_pNextPreviousButtonContainer);
        QHBoxLayout *pContainerLayout = new QHBoxLayout(m_pNextPreviousButtonContainer);
        /* Configure layout: */
#ifdef VBOX_WS_MAC
            pContainerLayout->setContentsMargins(5, 0, 5, 0);
            pContainerLayout->setSpacing(5);
#else
            pContainerLayout->setContentsMargins(qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 2, 0,
                                                 qApp->style()->pixelMetric(QStyle::PM_LayoutRightMargin) / 2, 0);
            pContainerLayout->setSpacing(qApp->style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing) / 2);
#endif
       m_pPreviousButton = new QIToolButton;
       if (m_pPreviousButton)
       {
           pContainerLayout->addWidget(m_pPreviousButton);
           m_pPreviousButton->setIcon(UIIconPool::iconSet(":/log_viewer_search_backward_16px.png"));
       }

       m_pNextButton = new QIToolButton;
       if (m_pNextButton){
           pContainerLayout->addWidget(m_pNextButton);
           m_pNextButton->setIcon(UIIconPool::iconSet(":/log_viewer_search_forward_16px.png"));
       }
    }

    /* Create case-sensitive checkbox: */
    m_pCaseSensitiveCheckBox = new QCheckBox;
    if (m_pCaseSensitiveCheckBox)
    {
        /* Configure font: */
#ifdef VBOX_DARWIN_USE_NATIVE_CONTROLS
        QFont font = m_pCaseSensitiveCheckBox->font();
        font.setPointSize(::darwinSmallFontSize());
        m_pCaseSensitiveCheckBox->setFont(font);
#endif /* VBOX_DARWIN_USE_NATIVE_CONTROLS */
        /* Add case-sensitive checkbox to main-layout: */
        mainLayout()->addWidget(m_pCaseSensitiveCheckBox);
    }

    m_pMatchWholeWordCheckBox = new QCheckBox;
    if (m_pMatchWholeWordCheckBox)
    {
        /* Configure focus for case-sensitive checkbox: */
        setFocusProxy(m_pMatchWholeWordCheckBox);
        /* Configure font: */
#ifdef VBOX_DARWIN_USE_NATIVE_CONTROLS
        QFont font = m_pMatchWholeWordCheckBox->font();
        font.setPointSize(::darwinSmallFontSize());
        m_pMatchWholeWordCheckBox->setFont(font);
#endif /* VBOX_DARWIN_USE_NATIVE_CONTROLS */
        mainLayout()->addWidget(m_pMatchWholeWordCheckBox);
    }

    m_pHighlightAllCheckBox = new QCheckBox;
    if (m_pHighlightAllCheckBox)
    {
        /* Configure font: */
#ifdef VBOX_DARWIN_USE_NATIVE_CONTROLS
        QFont font = m_pHighlightAllCheckBox->font();
        font.setPointSize(::darwinSmallFontSize());
        m_pHighlightAllCheckBox->setFont(font);
#endif /* VBOX_DARWIN_USE_NATIVE_CONTROLS */
        /* Add case-sensitive checkbox to main-layout: */
        mainLayout()->addWidget(m_pHighlightAllCheckBox);
    }

    /* Create warning-icon: */
    m_pWarningIcon = new QLabel;
    if (m_pWarningIcon)
    {
        /* Confifure warning-icon: */
        m_pWarningIcon->hide();
        QIcon icon = UIIconPool::defaultIcon(UIIconPool::UIDefaultIconType_MessageBoxWarning, this);
        if (!icon.isNull())
            m_pWarningIcon->setPixmap(icon.pixmap(16, 16));
        /* Add warning-icon to main-layout: */
        mainLayout()->addWidget(m_pWarningIcon);
    }

    /* Create warning-label: */
    m_pInfoLabel = new QLabel;
    if (m_pInfoLabel)
    {
        /* Configure warning-label: */
        m_pInfoLabel->hide();
        /* Prepare font: */
#ifdef VBOX_DARWIN_USE_NATIVE_CONTROLS
        QFont font = m_pInfoLabel->font();
        font.setPointSize(::darwinSmallFontSize());
        m_pInfoLabel->setFont(font);
#endif /* VBOX_DARWIN_USE_NATIVE_CONTROLS */
        /* Add warning-label to main-layout: */
        mainLayout()->addWidget(m_pInfoLabel);
    }
    mainLayout()->addStretch(2);
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
    if (m_pSearchLabel)
        m_pSearchLabel->setText(QString("%1 ").arg(UIVMLogViewerWidget::tr("&Find")));

    if (m_pSearchEditor)
        m_pSearchEditor->setToolTip(UIVMLogViewerWidget::tr("Enter a search string here"));

    if (m_pNextButton)
        m_pNextButton->setToolTip(UIVMLogViewerWidget::tr("Search for the next occurrence of the string (F3)"));

    if (m_pPreviousButton)
        m_pPreviousButton->setToolTip(UIVMLogViewerWidget::tr("Search for the previous occurrence of the string (Shift+F3)"));


    if (m_pCaseSensitiveCheckBox)
    {
        m_pCaseSensitiveCheckBox->setText(UIVMLogViewerWidget::tr("C&ase Sensitive"));
        m_pCaseSensitiveCheckBox->setToolTip(UIVMLogViewerWidget::tr("Perform case sensitive search (when checked)"));
    }

    if (m_pMatchWholeWordCheckBox)
    {
        m_pMatchWholeWordCheckBox->setText(UIVMLogViewerWidget::tr("Ma&tch Whole Word"));
        m_pMatchWholeWordCheckBox->setToolTip(UIVMLogViewerWidget::tr("Search matches only complete words when checked"));
    }

    if (m_pHighlightAllCheckBox)
    {
        m_pHighlightAllCheckBox->setText(UIVMLogViewerWidget::tr("&Highlight All"));
        m_pHighlightAllCheckBox->setToolTip(UIVMLogViewerWidget::tr("All occurence of the search text are highlighted"));
    }

    if (m_iMatchCount == 0)
        m_pInfoLabel->setText(UIVMLogViewerWidget::tr("String not found"));
    else if (m_iMatchCount > 0)
        m_pInfoLabel->setText(UIVMLogViewerWidget::tr("%1 Matches Found").arg(m_iMatchCount));
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
    {
        m_iMatchCount = -1;
        m_matchLocationVector.clear();
    }

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
            /* Set the match count 0 here since we did not call highLightAll function: */
            if (!m_pHighlightAllCheckBox->isChecked())
                m_iMatchCount = 0;
            configureInfoLabels();
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
    configureInfoLabels();
}

void UIVMLogViewerSearchPanel::findNext()
{
    search(ForwardSearch, false);
}

void UIVMLogViewerSearchPanel::findPrevious()
{
    search(BackwardSearch, false);
}

void UIVMLogViewerSearchPanel::clearHighlighting(int count)
{
    if (!viewer())
        return;
    m_iMatchCount = count;
    m_matchLocationVector.clear();

    QTextDocument* pDocument = textDocument();
    if (pDocument)
        pDocument->undo();

    configureInfoLabels();
    emit sigHighlightingUpdated();
}

void UIVMLogViewerSearchPanel::highlightAll(QTextDocument *pDocument,
                                            const QString &searchString)
{
    clearHighlighting(0);

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
    while (!highlightCursor.isNull() && !highlightCursor.atEnd())
    {
        /* Hightlighting searches is always from the top of the document forward: */
        highlightCursor = pDocument->find(searchString, highlightCursor, constructFindFlags(ForwardSearch));
        if (!highlightCursor.isNull())
        {
            highlightCursor.mergeCharFormat(colorFormat);
            ++m_iMatchCount;
            /* The following assumes we have single line blocks only: */
            int cursorLine = pDocument->findBlock(highlightCursor.position()).blockNumber();
            if (lineCount != 0)
                m_matchLocationVector.push_back(cursorLine / static_cast<float>(lineCount));
        }
    }
    cursor.endEditBlock();
}

void UIVMLogViewerSearchPanel::configureInfoLabels()
{
    if (!m_pSearchEditor || !m_pWarningIcon || !m_pInfoLabel)
        return;

    /* If no match has been found, mark the search editor: */
    if (m_iMatchCount == 0)
    {
        m_pSearchEditor->markError();
        m_pWarningIcon->setVisible(true);
        m_pInfoLabel->setVisible(true);
    }
    else if (m_iMatchCount == -1)
    {
        m_pSearchEditor->unmarkError();
        m_pWarningIcon->setVisible(false);
        m_pInfoLabel->setVisible(false);
    }
    else
    {
        m_pSearchEditor->unmarkError();
        m_pWarningIcon->setVisible(false);
        m_pInfoLabel->setVisible(true);
    }
    /* Retranslate to get the label text corectly: */
    retranslateUi();
}

QTextDocument::FindFlags UIVMLogViewerSearchPanel::constructFindFlags(SearchDirection eDirection)
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

