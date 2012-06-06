/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIVMLogViewer class implementation
 */

/*
 * Copyright (C) 2008-2011 Oracle Corporation
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
# include "precomp.h"
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QScrollBar>
#include <QTextEdit>

/* GUI includes: */
#include "UIVMLogViewer.h"
#include "QITabWidget.h"
#include "UIIconPool.h"
#include "UISpecialControls.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "VBoxUtils.h"

/* COM includes: */
#include "COMEnums.h"
#include "CSystemProperties.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* VM Log Viewer search panel: */
class UIVMLogViewerSearchPanel : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIVMLogViewerSearchPanel(QWidget *pParent, UIVMLogViewer *pViewer)
        : QIWithRetranslateUI<QWidget>(pParent)
        , m_pViewer(pViewer)
        , m_pCloseButton(0)
        , m_pSearchLabel(0), m_pSearchEditor(0)
        , m_pNextPrevButtons(0)
        , m_pCaseSensitiveCheckBox(0)
        , m_pWarningSpacer(0), m_pWarningIcon(0), m_pWarningLabel(0)
    {
        /* Close button: */
        m_pCloseButton = new UIMiniCancelButton(this);

        /* Search field: */
        m_pSearchLabel = new QLabel(this);
        m_pSearchEditor = new UISearchField(this);
        m_pSearchEditor->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        m_pSearchLabel->setBuddy(m_pSearchEditor);

        /* Next/Previous buttons: */
        m_pNextPrevButtons = new UIRoundRectSegmentedButton(2, this);
        m_pNextPrevButtons->setEnabled(0, false);
        m_pNextPrevButtons->setEnabled(1, false);
#ifndef Q_WS_MAC
        /* No icons on the Mac: */
        m_pNextPrevButtons->setIcon(0, UIIconPool::defaultIcon(UIIconPool::ArrowBackIcon, this));
        m_pNextPrevButtons->setIcon(1, UIIconPool::defaultIcon(UIIconPool::ArrowForwardIcon, this));
#endif /* !Q_WS_MAC */

        /* Case sensitive check-box: */
        m_pCaseSensitiveCheckBox = new QCheckBox(this);

        /* Warning label: */
        m_pWarningSpacer = new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
        m_pWarningIcon = new QLabel(this);
        m_pWarningIcon->hide();
        QIcon icon = UIIconPool::defaultIcon(UIIconPool::MessageBoxWarningIcon, this);
        if (!icon.isNull())
            m_pWarningIcon->setPixmap(icon.pixmap(16, 16));
        m_pWarningLabel = new QLabel(this);
        m_pWarningLabel->hide();
        QSpacerItem *pSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);

#ifdef VBOX_DARWIN_USE_NATIVE_CONTROLS
        QFont font = m_pSearchLabel->font();
        font.setPointSize(::darwinSmallFontSize());
        m_pSearchLabel->setFont(font);
        m_pCaseSensitiveCheckBox->setFont(font);
        m_pWarningLabel->setFont(font);
#endif /* VBOX_DARWIN_USE_NATIVE_CONTROLS */

        /* Placing widgets: */
        QHBoxLayout *pMainLayout = new QHBoxLayout(this);
        pMainLayout->setSpacing(5);
        pMainLayout->setContentsMargins(0, 0, 0, 0);
        pMainLayout->addWidget(m_pCloseButton);
        pMainLayout->addWidget(m_pSearchLabel);
        pMainLayout->addWidget(m_pSearchEditor);
        pMainLayout->addWidget(m_pNextPrevButtons);
        pMainLayout->addWidget(m_pCaseSensitiveCheckBox);
        pMainLayout->addItem(m_pWarningSpacer);
        pMainLayout->addWidget(m_pWarningIcon);
        pMainLayout->addWidget(m_pWarningLabel);
        pMainLayout->addItem(pSpacer);

        /* Setup focus proxy: */
        setFocusProxy(m_pCaseSensitiveCheckBox);

        /* Setup connections: */
        connect(m_pCloseButton, SIGNAL(clicked()), this, SLOT(hide()));
        connect(m_pSearchEditor, SIGNAL(textChanged(const QString &)),
                this, SLOT(findCurrent(const QString &)));
        connect(m_pNextPrevButtons, SIGNAL(clicked(int)), this, SLOT(find(int)));

        /* Retranslate finally: */
        retranslateUi();
    }

private slots:

    /* Slot to find specified tag,
     * called by next/previous buttons: */
    void find(int iButton)
    {
        if (iButton)
            findNext();
        else
            findBack();
    }

    /* Slot to find specified tag,
     * called when text changed in search editor: */
    void findCurrent(const QString &strSearchString)
    {
        m_pNextPrevButtons->setEnabled(0, strSearchString.length());
        m_pNextPrevButtons->setEnabled(1, strSearchString.length());
        toggleWarning(!strSearchString.length());
        if (strSearchString.length())
            search(true, true);
        else
        {
            QTextEdit *pBrowser = m_pViewer->currentLogPage();
            if (pBrowser && pBrowser->textCursor().hasSelection())
            {
                QTextCursor cursor = pBrowser->textCursor();
                cursor.setPosition(cursor.anchor());
                pBrowser->setTextCursor(cursor);
            }
        }
    }

private:

    /* Translation stuff: */
    void retranslateUi()
    {
        m_pCloseButton->setToolTip(UIVMLogViewer::tr("Close the search panel"));

        m_pSearchLabel->setText(QString("%1 ").arg(UIVMLogViewer::tr("&Find")));
        m_pSearchEditor->setToolTip(UIVMLogViewer::tr("Enter a search string here"));

        m_pNextPrevButtons->setTitle(0, UIVMLogViewer::tr("&Previous"));
        m_pNextPrevButtons->setToolTip(0, UIVMLogViewer::tr("Search for the previous occurrence of the string"));
        m_pNextPrevButtons->setTitle(1, UIVMLogViewer::tr("&Next"));
        m_pNextPrevButtons->setToolTip(1, UIVMLogViewer::tr("Search for the next occurrence of the string"));

        m_pCaseSensitiveCheckBox->setText(UIVMLogViewer::tr("C&ase Sensitive"));
        m_pCaseSensitiveCheckBox->setToolTip(UIVMLogViewer::tr("Perform case sensitive search (when checked)"));

        m_pWarningLabel->setText(UIVMLogViewer::tr("String not found"));
    }

    /* Key press filter: */
    void keyPressEvent(QKeyEvent *pEvent)
    {
        switch (pEvent->key())
        {
            /* Process Enter press as 'search next',
             * performed for any search panel widget: */
            case Qt::Key_Enter:
            case Qt::Key_Return:
            {
                if (pEvent->modifiers() == 0 ||
                    pEvent->modifiers() & Qt::KeypadModifier)
                {
                    m_pNextPrevButtons->animateClick(1);
                    return;
                }
                break;
            }
            default:
                break;
        }
        QWidget::keyPressEvent(pEvent);
    }

    /* Event filter, used for keyboard processing: */
    bool eventFilter(QObject *pObject, QEvent *pEvent)
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
                    if (pKeyEvent->QInputEvent::modifiers() == 0)
                    {
                        m_pNextPrevButtons->animateClick(1);
                        return true;
                    }
                    else if (pKeyEvent->QInputEvent::modifiers() == Qt::ShiftModifier)
                    {
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
                        if (isHidden())
                            show();
                        m_pSearchEditor->setFocus();
                        return true;
                    }
                }
                /* Handle alpha-numeric keys to implement the "find as you type" feature: */
                else if ((pKeyEvent->QInputEvent::modifiers() & ~Qt::ShiftModifier) == 0 &&
                         pKeyEvent->key() >= Qt::Key_Exclam && pKeyEvent->key() <= Qt::Key_AsciiTilde)
                {
                    if (m_pViewer->currentLogPage())
                    {
                        if (isHidden())
                            show();
                        m_pSearchEditor->setFocus();
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

    /* Show event reimplementation: */
    void showEvent(QShowEvent *pEvent)
    {
        QWidget::showEvent(pEvent);
        m_pSearchEditor->setFocus();
        m_pSearchEditor->selectAll();
    }

    /* Hide event reimplementation: */
    void hideEvent(QHideEvent *pEvent)
    {
        QWidget *pFocus = QApplication::focusWidget();
        if (pFocus && pFocus->parent() == this)
           focusNextPrevChild(true);
        QWidget::hideEvent(pEvent);
    }

    /* Search routine: */
    void search(bool fForward, bool fStartCurrent = false)
    {
        QTextEdit *pBrowser = m_pViewer->currentLogPage();
        if (!pBrowser) return;

        QTextCursor cursor = pBrowser->textCursor();
        int iPos = cursor.position();
        int iAnc = cursor.anchor();

        QString strText = pBrowser->toPlainText();
        int iDiff = fStartCurrent ? 0 : 1;

        int iResult = -1;
        if (fForward && (fStartCurrent || iPos < strText.size() - 1))
            iResult = strText.indexOf(m_pSearchEditor->text(), iAnc + iDiff,
                                      m_pCaseSensitiveCheckBox->isChecked() ?
                                      Qt::CaseSensitive : Qt::CaseInsensitive);
        else if (!fForward && iAnc > 0)
            iResult = strText.lastIndexOf(m_pSearchEditor->text(), iAnc - 1,
                                          m_pCaseSensitiveCheckBox->isChecked() ?
                                          Qt::CaseSensitive : Qt::CaseInsensitive);

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

        toggleWarning(iResult != -1);
    }

    /* Search routine wrappers: */
    void findNext() { search(true); }
    void findBack() { search(false); }

    /* Function to show/hide search border warning: */
    void toggleWarning(bool fHide)
    {
        m_pWarningSpacer->changeSize(fHide ? 0 : 16, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
        if (!fHide)
            m_pSearchEditor->markError();
        else
            m_pSearchEditor->unmarkError();
        m_pWarningIcon->setHidden(fHide);
        m_pWarningLabel->setHidden(fHide);
    }

    /* Widgets: */
    UIVMLogViewer *m_pViewer;
    UIMiniCancelButton *m_pCloseButton;
    QLabel *m_pSearchLabel;
    UISearchField *m_pSearchEditor;
    UIRoundRectSegmentedButton *m_pNextPrevButtons;
    QCheckBox *m_pCaseSensitiveCheckBox;
    QSpacerItem *m_pWarningSpacer;
    QLabel *m_pWarningIcon;
    QLabel *m_pWarningLabel;
};

/* VM Log Viewer array: */
VMLogViewerMap UIVMLogViewer::m_viewers = VMLogViewerMap();

void UIVMLogViewer::showLogViewerFor(QWidget *pCenterWidget, const CMachine &machine)
{
    /* If there is no corresponding VM Log Viewer created: */
    if (!m_viewers.contains(machine.GetName()))
    {
        /* Creating new VM Log Viewer: */
        UIVMLogViewer *pLogViewer = new UIVMLogViewer(pCenterWidget, Qt::Window, machine);
        pLogViewer->setAttribute(Qt::WA_DeleteOnClose);
        m_viewers[machine.GetName()] = pLogViewer;
    }

    /* Show VM Log Viewer: */
    UIVMLogViewer *pViewer = m_viewers[machine.GetName()];
    pViewer->show();
    pViewer->raise();
    pViewer->setWindowState(pViewer->windowState() & ~Qt::WindowMinimized);
    pViewer->activateWindow();
}

UIVMLogViewer::UIVMLogViewer(QWidget *pParent, Qt::WindowFlags flags, const CMachine &machine)
    : QIWithRetranslateUI2<QMainWindow>(pParent, flags)
    , m_fIsPolished(false)
    , m_machine(machine)
{
    /* Apply UI decorations: */
    Ui::UIVMLogViewer::setupUi(this);

    /* Apply window icons: */
    setWindowIcon(UIIconPool::iconSetFull(QSize(32, 32), QSize(16, 16),
                                          ":/vm_show_logs_32px.png",
                                          ":/show_logs_16px.png"));

    /* Create VM Log Vewer container: */
    m_pViewerContainer = new QITabWidget(centralWidget());
    m_pMainLayout->insertWidget(0, m_pViewerContainer);

    /* Create VM Log Vewer search panel: */
    m_pSearchPanel = new UIVMLogViewerSearchPanel(centralWidget(), this);
    centralWidget()->installEventFilter(m_pSearchPanel);
    m_pSearchPanel->hide();
    m_pMainLayout->insertWidget(1, m_pSearchPanel);

    /* Add missing buttons & retrieve standard buttons: */
    mBtnHelp = m_pButtonBox->button(QDialogButtonBox::Help);
    mBtnFind = m_pButtonBox->addButton(QString::null, QDialogButtonBox::ActionRole);
    mBtnRefresh = m_pButtonBox->addButton(QString::null, QDialogButtonBox::ActionRole);
    mBtnClose = m_pButtonBox->button(QDialogButtonBox::Close);
    mBtnSave = m_pButtonBox->button(QDialogButtonBox::Save);

    /* Setup connections: */
    connect(m_pButtonBox, SIGNAL(helpRequested()), &msgCenter(), SLOT(sltShowHelpHelpDialog()));
    connect(mBtnFind, SIGNAL(clicked()), this, SLOT(search()));
    connect(mBtnRefresh, SIGNAL(clicked()), this, SLOT(refresh()));
    connect(mBtnClose, SIGNAL(clicked()), this, SLOT(close()));
    connect(mBtnSave, SIGNAL(clicked()), this, SLOT(save()));

    /* Reading log files: */
    refresh();

    /* Loading language constants */
    retranslateUi();
}

UIVMLogViewer::~UIVMLogViewer()
{
    if (!m_machine.isNull())
        m_viewers.remove(m_machine.GetName());
}

QTextEdit* UIVMLogViewer::currentLogPage()
{
    if (m_pViewerContainer->isEnabled())
    {
        QWidget *pContainer = m_pViewerContainer->currentWidget();
        QTextEdit *pBrowser = pContainer->findChild<QTextEdit*>();
        Assert(pBrowser);
        return pBrowser ? pBrowser : 0;
    }
    else
        return 0;
}

void UIVMLogViewer::search()
{
    m_pSearchPanel->isHidden() ? m_pSearchPanel->show() : m_pSearchPanel->hide();
}

void UIVMLogViewer::refresh()
{
    /* Clearing old data if any: */
    m_book.clear();
    m_pViewerContainer->setEnabled(true);
    while (m_pViewerContainer->count())
    {
        QWidget *pFirstPage = m_pViewerContainer->widget(0);
        m_pViewerContainer->removeTab(0);
        delete pFirstPage;
    }

    bool isAnyLogPresent = false;

    const CSystemProperties &sys = vboxGlobal().virtualBox().GetSystemProperties();
    int cMaxLogs = sys.GetLogHistoryCount();
    for (int i=0; i <= cMaxLogs; ++i)
    {
        /* Query the log file name for index i: */
        QString strFileName = m_machine.QueryLogFilename(i);
        if (!strFileName.isEmpty())
        {
            /* Try to read the log file with the index i: */
            ULONG uOffset = 0;
            QString strText;
            while (true)
            {
                QVector<BYTE> data = m_machine.ReadLog(i, uOffset, _1M);
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
                /* Add the actual file name and the QTextEdit containing the content to a list: */
                m_book << qMakePair(strFileName, pLogViewer);
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
                              .arg(m_machine.GetLogFolder()));
        /* We don't want it to remain white: */
        QPalette pal = pDummyLog->palette();
        pal.setColor(QPalette::Base, pal.color(QPalette::Window));
        pDummyLog->setPalette(pal);
    }

    /* Show the first tab widget's page after the refresh: */
    m_pViewerContainer->setCurrentIndex(0);

    /* Enable/Disable save button & tab widget according log presence: */
    mBtnFind->setEnabled(isAnyLogPresent);
    mBtnSave->setEnabled(isAnyLogPresent);
    m_pViewerContainer->setEnabled(isAnyLogPresent);
}

bool UIVMLogViewer::close()
{
    m_pSearchPanel->hide();
    return QMainWindow::close();
}

void UIVMLogViewer::save()
{
    /* Prepare "save as" dialog: */
    QFileInfo fileInfo(m_book.at(m_pViewerContainer->currentIndex()).first);
    QDateTime dtInfo = fileInfo.lastModified();
    QString strDtString = dtInfo.toString("yyyy-MM-dd-hh-mm-ss");
    QString strDefaultFileName = QString("%1-%2.log").arg(m_machine.GetName()).arg(strDtString);
    QString strDefaultFullName = QDir::toNativeSeparators(QDir::home().absolutePath() + "/" + strDefaultFileName);
    QString strNewFileName = QFileDialog::getSaveFileName(this, tr("Save VirtualBox Log As"), strDefaultFullName);

    /* Copy log into the file: */
    if (!strNewFileName.isEmpty())
        QFile::copy(m_machine.QueryLogFilename(m_pViewerContainer->currentIndex()), strNewFileName);
}

void UIVMLogViewer::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIVMLogViewer::retranslateUi(this);

    /* Setup a dialog caption: */
    if (!m_machine.isNull())
        setWindowTitle(tr("%1 - VirtualBox Log Viewer").arg(m_machine.GetName()));

    /* Translate other tags: */
    mBtnFind->setText(tr("&Find"));
    mBtnRefresh->setText(tr("&Refresh"));
    mBtnSave->setText(tr("&Save"));
    mBtnClose->setText(tr("Close"));
}

void UIVMLogViewer::showEvent(QShowEvent *pEvent)
{
    QMainWindow::showEvent(pEvent);

    /* One may think that QWidget::polish() is the right place to do things
     * below, but apparently, by the time when QWidget::polish() is called,
     * the widget style & layout are not fully done, at least the minimum
     * size hint is not properly calculated. Since this is sometimes necessary,
     * we provide our own "polish" implementation. */

    if (m_fIsPolished)
        return;

    m_fIsPolished = true;

    /* Resize the whole log-viewer to fit 80 symbols in
     * text-browser for the first time started: */
    QTextEdit *pFirstPage = currentLogPage();
    if (pFirstPage)
    {
        int fullWidth = pFirstPage->fontMetrics().width(QChar('x')) * 80 +
                        pFirstPage->verticalScrollBar()->width() +
                        pFirstPage->frameWidth() * 2 +
                        /* m_pViewerContainer margin */ 10 * 2 +
                        /* CentralWidget margin */ 10 * 2;
        resize(fullWidth, height());
    }

    /* Make sure the log view widget has the focus */
    QWidget *pCurrentLogPage = currentLogPage();
    if (pCurrentLogPage)
        pCurrentLogPage->setFocus();

    /* Explicit widget centering relatively to it's parent: */
    VBoxGlobal::centerWidget(this, parentWidget(), false);
}

void UIVMLogViewer::keyPressEvent(QKeyEvent *pEvent)
{
    /* Depending on key pressed: */
    switch (pEvent->key())
    {
        /* Process key escape as VM Log Viewer close: */
        case Qt::Key_Escape:
        {
            mBtnClose->animateClick();
            return;
        }
        /* Precess Back key as switch to previous tab: */
        case Qt::Key_Back:
        {
            if (m_pViewerContainer->currentIndex() > 0)
            {
                m_pViewerContainer->setCurrentIndex(m_pViewerContainer->currentIndex() - 1);
                return;
            }
            break;
        }
        /* Precess Forward key as switch to next tab: */
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
    QMainWindow::keyReleaseEvent(pEvent);
}

QTextEdit* UIVMLogViewer::createLogPage(const QString &strName)
{
    QWidget *pPageContainer = new QWidget;
    QVBoxLayout *pPageLayout = new QVBoxLayout(pPageContainer);
    QTextEdit *pLogViewer = new QTextEdit(pPageContainer);
    pPageLayout->addWidget(pLogViewer);
    pPageLayout->setContentsMargins(10, 10, 10, 10);

    QFont font = pLogViewer->currentFont();
    font.setFamily("Courier New,courier");
    pLogViewer->setFont(font);
    pLogViewer->setWordWrapMode(QTextOption::NoWrap);
    pLogViewer->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    pLogViewer->setReadOnly(true);

    m_pViewerContainer->addTab(pPageContainer, strName);
    return pLogViewer;
}

#include "UIVMLogViewer.moc"

