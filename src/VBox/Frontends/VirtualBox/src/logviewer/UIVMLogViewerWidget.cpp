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
# include <QDateTime>
# include <QDir>
# include <QDialogButtonBox>
# include <QVBoxLayout>
# if defined(RT_OS_SOLARIS)
#  include <QFontDatabase>
# endif
# include <QPushButton>
# include <QTextEdit>

/* GUI includes: */
# include "QIFileDialog.h"
# include "QITabWidget.h"
# include "UIExtraDataManager.h"
# include "UIIconPool.h"
# include "UIMessageCenter.h"
# include "UIVMLogViewerWidget.h"
# include "UIVMLogViewerFilterPanel.h"
# include "UIVMLogViewerSearchPanel.h"

/* COM includes: */
# include "CSystemProperties.h"

# include "VBoxGlobal.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIVMLogViewerWidget::UIVMLogViewerWidget(QWidget *pParent, const CMachine &machine)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_fIsPolished(false)
    , m_comMachine(machine)
    , m_pButtonBox(0)
    , m_pMainLayout(0)
    , m_pButtonFind(0)
    , m_pButtonRefresh(0)
    , m_pButtonSave(0)
    , m_pButtonFilter(0)

{
    /* Prepare VM Log-Viewer: */
    prepare();
}

UIVMLogViewerWidget::~UIVMLogViewerWidget()
{
    /* Cleanup VM Log-Viewer: */
    cleanup();
}

int UIVMLogViewerWidget::currentLogPagewidth() const
{
    if(!currentLogPage())
        return 0;

    QTextDocument *pTextDocument = currentLogPage()->document();
    if(!pTextDocument)
        return 0;
    /* Adjust text-edit size: */
    pTextDocument->adjustSize();
    /* Get corresponding QTextDocument size: */
    QSize textSize = pTextDocument->size().toSize();
    return textSize.width();
}

bool UIVMLogViewerWidget::shouldBeMaximized() const
{
    return gEDataManager->logWindowShouldBeMaximized();
}

void UIVMLogViewerWidget::search()
{
    /* Show/hide search-panel: */
    m_pSearchPanel->isHidden() ? m_pSearchPanel->show() : m_pSearchPanel->hide();
}

void UIVMLogViewerWidget::refresh()
{
    /* Disconnect this connection to avoid initial signals during page creation/deletion: */
    disconnect(m_pViewerContainer, SIGNAL(currentChanged(int)), m_pFilterPanel, SLOT(applyFilter(int)));

    /* Clearing old data if any: */
    m_book.clear();
    m_logMap.clear();
    m_pViewerContainer->setEnabled(true);
    while (m_pViewerContainer->count())
    {
        QWidget *pFirstPage = m_pViewerContainer->widget(0);
        m_pViewerContainer->removeTab(0);
        delete pFirstPage;
    }

    bool isAnyLogPresent = false;

    const CSystemProperties &sys = vboxGlobal().virtualBox().GetSystemProperties();
    unsigned cMaxLogs = sys.GetLogHistoryCount() + 1 /*VBox.log*/ + 1 /*VBoxHardening.log*/; /** @todo Add api for getting total possible log count! */
    for (unsigned i = 0; i < cMaxLogs; ++i)
    {
        /* Query the log file name for index i: */
        QString strFileName = m_comMachine.QueryLogFilename(i);
        if (!strFileName.isEmpty())
        {
            /* Try to read the log file with the index i: */
            ULONG uOffset = 0;
            QString strText;
            while (true)
            {
                QVector<BYTE> data = m_comMachine.ReadLog(i, uOffset, _1M);
                if (data.size() == 0)
                    break;
                strText.append(QString::fromUtf8((char*)data.data(), data.size()));
                uOffset += data.size();
            }
            /* Anything read at all? */
            if (uOffset > 0)
            {
                /* Create a log viewer page and append the read text to it: */
                QTextEdit *pLogViewer = createLogPage(QFileInfo(strFileName).fileName());
                pLogViewer->setPlainText(strText);
                /* Move the cursor position to end: */
                QTextCursor cursor = pLogViewer->textCursor();
                cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
                pLogViewer->setTextCursor(cursor);
                /* Add the actual file name and the QTextEdit containing the content to a list: */
                m_book << qMakePair(strFileName, pLogViewer);
                /* Add the log-text to the map: */
                m_logMap[pLogViewer] = strText;
                isAnyLogPresent = true;
            }
        }
    }

    /* Create an empty log page if there are no logs at all: */
    if (!isAnyLogPresent)
    {
        QTextEdit *pDummyLog = createLogPage("VBox.log");
        pDummyLog->setWordWrapMode(QTextOption::WordWrap);
        pDummyLog->setHtml(tr("<p>No log files found. Press the "
                              "<b>Refresh</b> button to rescan the log folder "
                              "<nobr><b>%1</b></nobr>.</p>")
                              .arg(m_comMachine.GetLogFolder()));
        /* We don't want it to remain white: */
        QPalette pal = pDummyLog->palette();
        pal.setColor(QPalette::Base, pal.color(QPalette::Window));
        pDummyLog->setPalette(pal);
    }

    /* Show the first tab widget's page after the refresh: */
    m_pViewerContainer->setCurrentIndex(0);

    /* Apply the filter settings: */
    m_pFilterPanel->applyFilter();

    /* Setup this connection after refresh to avoid initial signals during page creation: */
    connect(m_pViewerContainer, SIGNAL(currentChanged(int)), m_pFilterPanel, SLOT(applyFilter(int)));

    /* Enable/Disable save button & tab widget according log presence: */
    m_pButtonFind->setEnabled(isAnyLogPresent);
    m_pButtonSave->setEnabled(isAnyLogPresent);
    m_pButtonFilter->setEnabled(isAnyLogPresent);
    m_pViewerContainer->setEnabled(isAnyLogPresent);
}

void UIVMLogViewerWidget::save()
{
    /* Prepare "save as" dialog: */
    const QFileInfo fileInfo(m_book.at(m_pViewerContainer->currentIndex()).first);
    /* Prepare default filename: */
    const QDateTime dtInfo = fileInfo.lastModified();
    const QString strDtString = dtInfo.toString("yyyy-MM-dd-hh-mm-ss");
    const QString strDefaultFileName = QString("%1-%2.log").arg(m_comMachine.GetName()).arg(strDtString);
    const QString strDefaultFullName = QDir::toNativeSeparators(QDir::home().absolutePath() + "/" + strDefaultFileName);
    /* Show "save as" dialog: */
    const QString strNewFileName = QIFileDialog::getSaveFileName(strDefaultFullName,
                                                                 "",
                                                                 this,
                                                                 tr("Save VirtualBox Log As"),
                                                                 0 /* selected filter */,
                                                                 true /* resolve symlinks */,
                                                                 true /* confirm overwrite */);
    /* Make sure file-name is not empty: */
    if (!strNewFileName.isEmpty())
    {
        /* Delete the previous file if already exists as user already confirmed: */
        if (QFile::exists(strNewFileName))
            QFile::remove(strNewFileName);
        /* Copy log into the file: */
        QFile::copy(m_comMachine.QueryLogFilename(m_pViewerContainer->currentIndex()), strNewFileName);
    }
}

void UIVMLogViewerWidget::filter()
{
    /* Show/hide filter-panel: */
    m_pFilterPanel->isHidden() ? m_pFilterPanel->show() : m_pFilterPanel->hide();
}

void UIVMLogViewerWidget::prepare()
{
    m_pButtonBox = new QDialogButtonBox(this);
    m_pMainLayout = new QVBoxLayout(this);

    m_pMainLayout->addWidget(m_pButtonBox);

    /* Apply window icons: */
    setWindowIcon(UIIconPool::iconSetFull(":/vm_show_logs_32px.png", ":/vm_show_logs_16px.png"));

    /* Prepare widgets: */
    prepareWidgets();

    /* Prepare connections: */
    prepareConnections();

    /* Reading log files: */
    refresh();

    /* Loading language constants: */
    retranslateUi();
}

void UIVMLogViewerWidget::prepareWidgets()
{
    /* Create VM Log-Viewer container: */
    m_pViewerContainer = new QITabWidget(this);
    AssertPtrReturnVoid(m_pViewerContainer);
    {
        /* Add VM Log-Viewer container to main-layout: */
        m_pMainLayout->insertWidget(0, m_pViewerContainer);
    }

    /* Create VM Log-Viewer search-panel: */
    m_pSearchPanel = new UIVMLogViewerSearchPanel(this, this);
    AssertPtrReturnVoid(m_pSearchPanel);
    {
        /* Configure VM Log-Viewer search-panel: */
        installEventFilter(m_pSearchPanel);
        m_pSearchPanel->hide();
        /* Add VM Log-Viewer search-panel to main-layout: */
        m_pMainLayout->insertWidget(1, m_pSearchPanel);
    }

    /* Create VM Log-Viewer filter-panel: */
    m_pFilterPanel = new UIVMLogViewerFilterPanel(this, this);
    AssertPtrReturnVoid(m_pFilterPanel);
    {
        /* Configure VM Log-Viewer filter-panel: */
        installEventFilter(m_pFilterPanel);
        m_pFilterPanel->hide();
        /* Add VM Log-Viewer filter-panel to main-layout: */
        m_pMainLayout->insertWidget(2, m_pFilterPanel);
    }

    /* Create find-button: */
    m_pButtonFind = m_pButtonBox->addButton(QString::null, QDialogButtonBox::ActionRole);
    AssertPtrReturnVoid(m_pButtonFind);
    /* Create filter-button: */
    m_pButtonFilter = m_pButtonBox->addButton(QString::null, QDialogButtonBox::ActionRole);
    AssertPtrReturnVoid(m_pButtonFilter);

    /* Create save-button: */
    m_pButtonSave = m_pButtonBox->addButton(QDialogButtonBox::Save);
    AssertPtrReturnVoid(m_pButtonSave);
    /* Create refresh-button: */
    m_pButtonRefresh = m_pButtonBox->addButton(QString::null, QDialogButtonBox::ActionRole);
    AssertPtrReturnVoid(m_pButtonRefresh);
}

void UIVMLogViewerWidget::prepareConnections()
{
    /* Prepare connections: */
    connect(m_pButtonBox, SIGNAL(helpRequested()), &msgCenter(), SLOT(sltShowHelpHelpDialog()));
    connect(m_pButtonFind, SIGNAL(clicked()), this, SLOT(search()));
    connect(m_pButtonRefresh, SIGNAL(clicked()), this, SLOT(refresh()));
    connect(m_pButtonSave, SIGNAL(clicked()), this, SLOT(save()));
    connect(m_pButtonFilter, SIGNAL(clicked()), this, SLOT(filter()));
}

void UIVMLogViewerWidget::cleanup()
{
}

void UIVMLogViewerWidget::retranslateUi()
{
    /* Setup a dialog caption: */
    if (!m_comMachine.isNull())
        setWindowTitle(tr("%1 - VirtualBox Log Viewer").arg(m_comMachine.GetName()));

    /* Translate other tags: */
    m_pButtonFind->setText(tr("&Find"));
    m_pButtonRefresh->setText(tr("&Refresh"));
    m_pButtonSave->setText(tr("&Save"));
    m_pButtonFilter->setText(tr("Fil&ter"));
}

void UIVMLogViewerWidget::showEvent(QShowEvent *pEvent)
{
    QWidget::showEvent(pEvent);

    /* One may think that QWidget::polish() is the right place to do things
     * below, but apparently, by the time when QWidget::polish() is called,
     * the widget style & layout are not fully done, at least the minimum
     * size hint is not properly calculated. Since this is sometimes necessary,
     * we provide our own "polish" implementation: */

    if (m_fIsPolished)
        return;

    m_fIsPolished = true;

    /* Make sure the log view widget has the focus: */
    QWidget *pCurrentLogPage = currentLogPage();
    if (pCurrentLogPage)
        pCurrentLogPage->setFocus();
}

void UIVMLogViewerWidget::keyPressEvent(QKeyEvent *pEvent)
{
    /* Depending on key pressed: */
    switch (pEvent->key())
    {
        /* Process key escape as VM Log Viewer close: */
        case Qt::Key_Escape:
        {
            return;
        }
        /* Process Back key as switch to previous tab: */
        case Qt::Key_Back:
        {
            if (m_pViewerContainer->currentIndex() > 0)
            {
                m_pViewerContainer->setCurrentIndex(m_pViewerContainer->currentIndex() - 1);
                return;
            }
            break;
        }
        /* Process Forward key as switch to next tab: */
        case Qt::Key_Forward:
        {
            if (m_pViewerContainer->currentIndex() < m_pViewerContainer->count())
            {
                m_pViewerContainer->setCurrentIndex(m_pViewerContainer->currentIndex() + 1);
                return;
            }
            break;
        }
        default:
            break;
    }
    QWidget::keyReleaseEvent(pEvent);
}

QTextEdit* UIVMLogViewerWidget::currentLogPage() const
{
    /* If viewer-container is enabled: */
    if (m_pViewerContainer->isEnabled())
    {
        /* Get and return current log-page: */
        QWidget *pContainer = m_pViewerContainer->currentWidget();
        QTextEdit *pBrowser = pContainer->findChild<QTextEdit*>();
        Assert(pBrowser);
        return pBrowser ? pBrowser : 0;
    }
    /* Return NULL by default: */
    return 0;
}

QTextEdit* UIVMLogViewerWidget::createLogPage(const QString &strName)
{
    /* Create page-container: */
    QWidget *pPageContainer = new QWidget;
    AssertPtrReturn(pPageContainer, 0);
    {
        /* Create page-layout: */
        QVBoxLayout *pPageLayout = new QVBoxLayout(pPageContainer);
        AssertPtrReturn(pPageLayout, 0);
        /* Create Log-Viewer: */
        QTextEdit *pLogViewer = new QTextEdit(pPageContainer);
        AssertPtrReturn(pLogViewer, 0);
        {
            /* Configure Log-Viewer: */
#if defined(RT_OS_SOLARIS)
            /* Use system fixed-width font on Solaris hosts as the Courier family fonts don't render well. */
            QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
#else
            QFont font = pLogViewer->currentFont();
            font.setFamily("Courier New,courier");
#endif
            pLogViewer->setFont(font);
            pLogViewer->setWordWrapMode(QTextOption::NoWrap);
            pLogViewer->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
            pLogViewer->setReadOnly(true);
            /* Add Log-Viewer to page-layout: */
            pPageLayout->addWidget(pLogViewer);
        }
        /* Add page-container to viewer-container: */
        m_pViewerContainer->addTab(pPageContainer, strName);
        return pLogViewer;
    }
}

const QString& UIVMLogViewerWidget::currentLog()
{
    return m_logMap[currentLogPage()];
}
