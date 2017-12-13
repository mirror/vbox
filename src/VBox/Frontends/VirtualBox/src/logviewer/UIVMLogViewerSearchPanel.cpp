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
# include <QCheckBox>
# include <QComboBox>
# if defined(RT_OS_SOLARIS)
#  include <QFontDatabase>
# endif
# include <QHBoxLayout>
# include <QLabel>
# include <QPlainTextEdit>

/* GUI includes: */
# include "UIIconPool.h"
# include "UISpecialControls.h"
# include "UIVMLogViewerSearchPanel.h"
# include "UIVMLogViewerWidget.h"
# ifdef VBOX_WS_MAC
#  include "VBoxUtils-darwin.h"
# endif

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIVMLogViewerSearchPanel::UIVMLogViewerSearchPanel(QWidget *pParent, UIVMLogViewerWidget *pViewer)
: QIWithRetranslateUI<QWidget>(pParent)
    , m_pViewer(pViewer)
    , m_pMainLayout(0)
    , m_pCloseButton(0)
    , m_pSearchLabel(0), m_pSearchEditor(0)
    , m_pNextPrevButtons(0)
    , m_pCaseSensitiveCheckBox(0)
    , m_pWarningSpacer(0), m_pWarningIcon(0), m_pWarningLabel(0)
{
    /* Prepare: */
    prepare();
}


void UIVMLogViewerSearchPanel::find(int iButton)
{
    /* If button-id is 1, findNext: */
    if (iButton)
        findNext();
    /* If button-id is 0, findBack: */
    else
        findBack();
}

void UIVMLogViewerSearchPanel::findCurrent(const QString &strSearchString)
{
    /* Enable/disable Next-Previous buttons as per search-string validity: */
    m_pNextPrevButtons->setEnabled(0, strSearchString.length());
    m_pNextPrevButtons->setEnabled(1, strSearchString.length());
    /* Show/hide the warning as per search-string validity: */
    toggleWarning(!strSearchString.length());
    /* If search-string is valid: */
    if (strSearchString.length())
        search(true, true);
    /* If search-string is not valid, reset cursor position: */
    else
    {
        /* Get current log-page: */
        QPlainTextEdit *pBrowser = m_pViewer->currentLogPage();
        /* If current log-page is valid and cursor has selection: */
        if (pBrowser && pBrowser->textCursor().hasSelection())
        {
            /* Get cursor and reset position: */
            QTextCursor cursor = pBrowser->textCursor();
            cursor.setPosition(cursor.anchor());
            pBrowser->setTextCursor(cursor);
        }
    }
}

void UIVMLogViewerSearchPanel::prepare()
{
    /* Prepare widgets: */
    prepareWidgets();

    /* Prepare connections: */
    prepareConnections();

    /* Retranslate finally: */
    retranslateUi();
}

void UIVMLogViewerSearchPanel::prepareWidgets()
{
    /* Create main-layout: */
    m_pMainLayout = new QHBoxLayout(this);
    AssertPtrReturnVoid(m_pMainLayout);
    {
        /* Configure main-layout: */
        m_pMainLayout->setContentsMargins(0, 0, 0, 0);

        /* Create close-button: */
        m_pCloseButton = new UIMiniCancelButton;
        AssertPtrReturnVoid(m_pCloseButton);
        {
            /* Add close-button to main-layout: */
            m_pMainLayout->addWidget(m_pCloseButton);
        }

        /* Create search-editor: */
        m_pSearchEditor = new UISearchField(this);
        AssertPtrReturnVoid(m_pSearchEditor);
        {
            /* Configure search-editor: */
            m_pSearchEditor->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            /* Add search-editor to main-layout: */
            m_pMainLayout->addWidget(m_pSearchEditor);
        }

        /* Create search-label: */
        m_pSearchLabel = new QLabel;
        AssertPtrReturnVoid(m_pSearchLabel);
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
            m_pMainLayout->addWidget(m_pSearchLabel);
        }

        /* Create Next/Prev button-box: */
        m_pNextPrevButtons = new UIRoundRectSegmentedButton(this, 2);
        AssertPtrReturnVoid(m_pNextPrevButtons);
        {
            /* Configure Next/Prev button-box: */
            m_pNextPrevButtons->setEnabled(0, false);
            m_pNextPrevButtons->setEnabled(1, false);
#ifndef VBOX_WS_MAC
            /* No icons on the Mac: */
            m_pNextPrevButtons->setIcon(0, UIIconPool::defaultIcon(UIIconPool::UIDefaultIconType_ArrowBack, this));
            m_pNextPrevButtons->setIcon(1, UIIconPool::defaultIcon(UIIconPool::UIDefaultIconType_ArrowForward, this));
#endif /* !VBOX_WS_MAC */
            /* Add Next/Prev button-box to main-layout: */
            m_pMainLayout->addWidget(m_pNextPrevButtons);
        }

        /* Create case-sensitive checkbox: */
        m_pCaseSensitiveCheckBox = new QCheckBox;
        AssertPtrReturnVoid(m_pCaseSensitiveCheckBox);
        {
            /* Configure focus for case-sensitive checkbox: */
            setFocusProxy(m_pCaseSensitiveCheckBox);
            /* Configure font: */
#ifdef VBOX_DARWIN_USE_NATIVE_CONTROLS
            QFont font = m_pCaseSensitiveCheckBox->font();
            font.setPointSize(::darwinSmallFontSize());
            m_pCaseSensitiveCheckBox->setFont(font);
#endif /* VBOX_DARWIN_USE_NATIVE_CONTROLS */
            /* Add case-sensitive checkbox to main-layout: */
            m_pMainLayout->addWidget(m_pCaseSensitiveCheckBox);
        }

        /* Create warning-spacer: */
        m_pWarningSpacer = new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
        AssertPtrReturnVoid(m_pWarningSpacer);
        {
            /* Add warning-spacer to main-layout: */
            m_pMainLayout->addItem(m_pWarningSpacer);
        }

        /* Create warning-icon: */
        m_pWarningIcon = new QLabel;
        AssertPtrReturnVoid(m_pWarningIcon);
        {
            /* Confifure warning-icon: */
            m_pWarningIcon->hide();
            QIcon icon = UIIconPool::defaultIcon(UIIconPool::UIDefaultIconType_MessageBoxWarning, this);
            if (!icon.isNull())
                m_pWarningIcon->setPixmap(icon.pixmap(16, 16));
            /* Add warning-icon to main-layout: */
            m_pMainLayout->addWidget(m_pWarningIcon);
        }

        /* Create warning-label: */
        m_pWarningLabel = new QLabel;
        AssertPtrReturnVoid(m_pWarningLabel);
        {
            /* Configure warning-label: */
            m_pWarningLabel->hide();
            /* Prepare font: */
#ifdef VBOX_DARWIN_USE_NATIVE_CONTROLS
            QFont font = m_pWarningLabel->font();
            font.setPointSize(::darwinSmallFontSize());
            m_pWarningLabel->setFont(font);
#endif /* VBOX_DARWIN_USE_NATIVE_CONTROLS */
            /* Add warning-label to main-layout: */
            m_pMainLayout->addWidget(m_pWarningLabel);
        }

        /* Create spacer-item: */
        m_pSpacerItem = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
        AssertPtrReturnVoid(m_pSpacerItem);
        {
            /* Add spacer-item to main-layout: */
            m_pMainLayout->addItem(m_pSpacerItem);
        }
    }
}

void UIVMLogViewerSearchPanel::prepareConnections()
{
    /* Prepare connections: */
    connect(m_pCloseButton, SIGNAL(clicked()), this, SLOT(hide()));
    connect(m_pSearchEditor, SIGNAL(textChanged(const QString &)),
            this, SLOT(findCurrent(const QString &)));
    connect(m_pNextPrevButtons, SIGNAL(clicked(int)), this, SLOT(find(int)));
}

void UIVMLogViewerSearchPanel::retranslateUi()
{
    m_pCloseButton->setToolTip(UIVMLogViewerWidget::tr("Close the search panel"));

    m_pSearchLabel->setText(QString("%1 ").arg(UIVMLogViewerWidget::tr("&Find")));
    m_pSearchEditor->setToolTip(UIVMLogViewerWidget::tr("Enter a search string here"));

    m_pNextPrevButtons->setTitle(0, UIVMLogViewerWidget::tr("&Previous"));
    m_pNextPrevButtons->setToolTip(0, UIVMLogViewerWidget::tr("Search for the previous occurrence of the string"));
    m_pNextPrevButtons->setTitle(1, UIVMLogViewerWidget::tr("&Next"));
    m_pNextPrevButtons->setToolTip(1, UIVMLogViewerWidget::tr("Search for the next occurrence of the string"));

    m_pCaseSensitiveCheckBox->setText(UIVMLogViewerWidget::tr("C&ase Sensitive"));
    m_pCaseSensitiveCheckBox->setToolTip(UIVMLogViewerWidget::tr("Perform case sensitive search (when checked)"));

    m_pWarningLabel->setText(UIVMLogViewerWidget::tr("String not found"));
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
                m_pNextPrevButtons->animateClick(1);
                return;
            }
            break;
        }
    default:
        break;
    }
    /* Call to base-class: */
    QWidget::keyPressEvent(pEvent);
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
                    m_pNextPrevButtons->animateClick(1);
                    return true;
                }
                /* If there is 'ShiftModifier' 'Shift + Key-F3' is pressed: */
                else if (pKeyEvent->QInputEvent::modifiers() == Qt::ShiftModifier)
                {
                    /* Animate click on 'Prev' button: */
                    m_pNextPrevButtons->animateClick(0);
                    return true;
                }
            }
            /* Handle Ctrl+F key combination as a shortcut to focus search field: */
            else if (pKeyEvent->QInputEvent::modifiers() == Qt::ControlModifier &&
                     pKeyEvent->key() == Qt::Key_F)
            {
                if (m_pViewer->currentLogPage())
                {
                    /* Make sure current log-page is visible: */
                    if (isHidden())
                        show();
                    /* Set focus on search-editor: */
                    m_pSearchEditor->setFocus();
                    return true;
                }
            }
            /* Handle alpha-numeric keys to implement the "find as you type" feature: */
            else if ((pKeyEvent->QInputEvent::modifiers() & ~Qt::ShiftModifier) == 0 &&
                     pKeyEvent->key() >= Qt::Key_Exclam && pKeyEvent->key() <= Qt::Key_AsciiTilde)
            {
                /* Make sure current log-page is visible: */
                if (m_pViewer->currentLogPage())
                {
                    if (isHidden())
                        show();
                    /* Set focus on search-editor: */
                    m_pSearchEditor->setFocus();
                    /* Insert the text to search-editor, which triggers the search-operation for new text: */
                    m_pSearchEditor->insert(pKeyEvent->text());
                    return true;
                }
            }
            break;
        }
    default:
        break;
    }
    /* Call to base-class: */
    return QWidget::eventFilter(pObject, pEvent);
}

void UIVMLogViewerSearchPanel::showEvent(QShowEvent *pEvent)
{
    /* Call to base-class: */
    QWidget::showEvent(pEvent);
    /* Set focus on search-editor: */
    m_pSearchEditor->setFocus();
    /* Select all the text: */
    m_pSearchEditor->selectAll();
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
    QWidget::hideEvent(pEvent);
}

void UIVMLogViewerSearchPanel::search(bool fForward, bool fStartCurrent /* = false */)
{
    /* Get current log-page: */
    QPlainTextEdit *pBrowser = m_pViewer->currentLogPage();
    if (!pBrowser) return;

    /* Get text-cursor and its attributes: */
    QTextCursor cursor = pBrowser->textCursor();
    int iPos = cursor.position();
    int iAnc = cursor.anchor();

    /* Get the text to be searched: */
    QString strText = pBrowser->toPlainText();
    int iDiff = fStartCurrent ? 0 : 1;

    int iResult = -1;
    /* If search-direction is forward and cursor position is valid: */
    if (fForward && (fStartCurrent || iPos < strText.size() - 1))
    {
        /* Search and get the index of first match: */
        iResult = strText.indexOf(m_pSearchEditor->text(), iAnc + iDiff,
                                  m_pCaseSensitiveCheckBox->isChecked() ?
                                  Qt::CaseSensitive : Qt::CaseInsensitive);

        /* When searchstring is changed, search from beginning of log again: */
        if (iResult == -1 && fStartCurrent)
            iResult = strText.indexOf(m_pSearchEditor->text(), 0,
                                      m_pCaseSensitiveCheckBox->isChecked() ?
                                      Qt::CaseSensitive : Qt::CaseInsensitive);
    }
    /* If search-direction is backward: */
    else if (!fForward && iAnc > 0)
        iResult = strText.lastIndexOf(m_pSearchEditor->text(), iAnc - 1,
                                      m_pCaseSensitiveCheckBox->isChecked() ?
                                      Qt::CaseSensitive : Qt::CaseInsensitive);

    /* If the result is valid, move cursor-position: */
    if (iResult != -1)
    {
        cursor.movePosition(QTextCursor::Start,
                            QTextCursor::MoveAnchor);
        cursor.movePosition(QTextCursor::NextCharacter,
                            QTextCursor::MoveAnchor, iResult);
        cursor.movePosition(QTextCursor::NextCharacter,
                            QTextCursor::KeepAnchor,
                            m_pSearchEditor->text().size());
        pBrowser->setTextCursor(cursor);
    }

    /* Show/hide the warning as per search-result: */
    toggleWarning(iResult != -1);
}

void UIVMLogViewerSearchPanel::findNext()
{
    search(true);
}

void UIVMLogViewerSearchPanel::findBack()
{
    search(false);
}

void UIVMLogViewerSearchPanel::toggleWarning(bool fHide)
{
    /* Adjust size of warning-spacer accordingly: */
    m_pWarningSpacer->changeSize(fHide ? 0 : 16, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
    /* If visible mark the error for search-editor (changes text-color): */
    if (!fHide)
        m_pSearchEditor->markError();
    /* If hidden unmark the error for search-editor (changes text-color): */
    else
        m_pSearchEditor->unmarkError();
    /* Show/hide the warning-icon: */
    m_pWarningIcon->setHidden(fHide);
    /* Show/hide the warning-label: */
    m_pWarningLabel->setHidden(fHide);
}
