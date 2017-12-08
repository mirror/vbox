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
# include <QComboBox>
# include <QHBoxLayout>
# if defined(RT_OS_SOLARIS)
#  include <QFontDatabase>
# endif
# include <QLabel>
# include <QTextEdit>
# include <QTextCursor>

/* GUI includes: */
# include "UIVMLogViewerFilterPanel.h"
# include "UISpecialControls.h"
# include "UIVMLogViewerWidget.h"
# ifdef VBOX_WS_MAC
#  include "VBoxUtils-darwin.h"
# endif
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIVMLogViewerFilterPanel::UIVMLogViewerFilterPanel(QWidget *pParent, UIVMLogViewerWidget *pViewer)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pViewer(pViewer)
    , m_pMainLayout(0)
    , m_pCloseButton(0)
    , m_pFilterLabel(0), m_pFilterComboBox(0)
{
    /* Prepare: */
    prepare();
}

void UIVMLogViewerFilterPanel::applyFilter(const int iCurrentIndex /* = 0 */)
{
    Q_UNUSED(iCurrentIndex);
    QTextEdit *pCurrentPage = m_pViewer->currentLogPage();
    AssertReturnVoid(pCurrentPage);
    QString strInputText = m_pViewer->currentLog();
    if (!strInputText.isNull())
    {
        /* Prepare filter-data: */
        QString strFilteredText;
        const QRegExp rxFilterExp(m_strFilterText, Qt::CaseInsensitive);

        /* If filter regular-expression is not empty and valid, filter the log: */
        if (!rxFilterExp.isEmpty() && rxFilterExp.isValid())
        {
            while (!strInputText.isEmpty())
            {
                /* Read each line and check if it matches regular-expression: */
                const int index = strInputText.indexOf('\n');
                if (index > 0)
                {
                    QString strLine = strInputText.left(index + 1);
                    if (strLine.contains(rxFilterExp))
                        strFilteredText.append(strLine);
                }
                strInputText.remove(0, index + 1);
            }
            pCurrentPage->setPlainText(strFilteredText);
        }
        /* Restore entire log when filter regular expression is empty or not valid: */
        else
            pCurrentPage->setPlainText(strInputText);

        /* Move the cursor position to end: */
        QTextCursor cursor = pCurrentPage->textCursor();
        cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
        pCurrentPage->setTextCursor(cursor);
    }
}

void UIVMLogViewerFilterPanel::filter(const QString &strSearchString)
{
    m_strFilterText = strSearchString;
    applyFilter();
}

void UIVMLogViewerFilterPanel::prepare()
{
    /* Prepare widgets: */
    prepareWidgets();

    /* Prepare connections: */
    prepareConnections();

    /* Retranslate finally: */
    retranslateUi();
}

void UIVMLogViewerFilterPanel::prepareWidgets()
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

        /* Create filter-combobox: */
        m_pFilterComboBox = new QComboBox;
        AssertPtrReturnVoid(m_pFilterComboBox);
        {
            /* Configure filter-combobox: */
            m_pFilterComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            m_pFilterComboBox->setEditable(true);
            QStringList strFilterPresets;
            strFilterPresets << "" << "GUI" << "NAT" << "AHCI" << "VD" << "Audio" << "VUSB" << "SUP" << "PGM" << "HDA"
                             << "HM" << "VMM" << "GIM" << "CPUM";
            strFilterPresets.sort();
            m_pFilterComboBox->addItems(strFilterPresets);
            /* Add filter-combobox to main-layout: */
            m_pMainLayout->addWidget(m_pFilterComboBox);
        }

        /* Create filter-label: */
        m_pFilterLabel = new QLabel;
        AssertPtrReturnVoid(m_pFilterLabel);
        {
            /* Configure filter-label: */
            m_pFilterLabel->setBuddy(m_pFilterComboBox);
#ifdef VBOX_DARWIN_USE_NATIVE_CONTROLS
            QFont font = m_pFilterLabel->font();
            font.setPointSize(::darwinSmallFontSize());
            m_pFilterLabel->setFont(font);
#endif /* VBOX_DARWIN_USE_NATIVE_CONTROLS */
            /* Add filter-label to main-layout: */
            m_pMainLayout->addWidget(m_pFilterLabel);
        }
    }
}

void UIVMLogViewerFilterPanel::prepareConnections()
{
    /* Prepare connections: */
    connect(m_pCloseButton, SIGNAL(clicked()), this, SLOT(hide()));
    connect(m_pFilterComboBox, SIGNAL(editTextChanged(const QString &)),
            this, SLOT(filter(const QString &)));
}

void UIVMLogViewerFilterPanel::retranslateUi()
{
    m_pCloseButton->setToolTip(UIVMLogViewerWidget::tr("Close the search panel"));
    m_pFilterLabel->setText(UIVMLogViewerWidget::tr("Filter"));
    m_pFilterComboBox->setToolTip(UIVMLogViewerWidget::tr("Enter filtering string here"));
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
