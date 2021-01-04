/* $Id$ */
/** @file
 * VBox Qt GUI - UIHelpBrowserWidget class implementation.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
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
#include <QClipboard>
#include <QtGlobal>
#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP) && (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))
 #include <QtHelp/QHelpEngine>
 #include <QtHelp/QHelpContentWidget>
 #include <QtHelp/QHelpIndexWidget>
 #include <QtHelp/QHelpSearchEngine>
 #include <QtHelp/QHelpSearchQueryWidget>
 #include <QtHelp/QHelpSearchResultWidget>
#endif
#include <QLabel>
#include <QMenu>
#include <QHBoxLayout>
#include <QWidgetAction>
#ifdef RT_OS_SOLARIS
# include <QFontDatabase>
#endif

/* GUI includes: */
#include "QIToolButton.h"
#include "UIHelpViewer.h"
#include "UIHelpBrowserWidget.h"
#include "UIIconPool.h"
#include "UISearchLineEdit.h"

/* COM includes: */
#include "CSystemProperties.h"

#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP) && (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))


/*********************************************************************************************************************************
*   UIContextMenuNavigationAction definition.                                                                                    *
*********************************************************************************************************************************/
class UIContextMenuNavigationAction : public QWidgetAction
{

    Q_OBJECT;

signals:

    void sigGoBackward();
    void sigGoForward();
    void sigGoHome();
    void sigAddBookmark();

public:

    UIContextMenuNavigationAction(QObject *pParent = 0);
    void setBackwardAvailable(bool fAvailable);
    void setForwardAvailable(bool fAvailable);

protected:


private slots:


private:

    void prepare();
    QIToolButton *m_pBackwardButton;
    QIToolButton *m_pForwardButton;
    QIToolButton *m_pHomeButton;
    QIToolButton *m_pAddBookmarkButton;
};

/*********************************************************************************************************************************
*   UIFindInPageWidget definition.                                                                                        *
*********************************************************************************************************************************/
class UIFindInPageWidget : public QIWithRetranslateUI<QWidget>
{

    Q_OBJECT;

signals:

    void sigDragging(const QPoint &delta);
    void sigSearchTextChanged(const QString &strSearchText);
    void sigSelectNextMatch();
    void sigSelectPreviousMatch();
    void sigClose();

public:

    UIFindInPageWidget(QWidget *pParent = 0);
    void setMatchCountAndCurrentIndex(int iTotalMatchCount, int iCurrentlyScrolledIndex);
    void clearSearchField();

protected:

    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;

private:

    void prepare();
    void retranslateUi();
    UISearchLineEdit  *m_pSearchLineEdit;
    QIToolButton      *m_pNextButton;
    QIToolButton      *m_pPreviousButton;
    QIToolButton      *m_pCloseButton;
    QLabel            *m_pDragMoveLabel;
    QPoint m_previousMousePosition;
};


/*********************************************************************************************************************************
*   UIContextMenuNavigationAction implementation.                                                                                *
*********************************************************************************************************************************/
UIContextMenuNavigationAction::UIContextMenuNavigationAction(QObject *pParent /* = 0 */)
    :QWidgetAction(pParent)
    , m_pBackwardButton(0)
    , m_pForwardButton(0)
    , m_pHomeButton(0)
    , m_pAddBookmarkButton(0)
{
    prepare();
}

void UIContextMenuNavigationAction::setBackwardAvailable(bool fAvailable)
{
    if (m_pBackwardButton)
        m_pBackwardButton->setEnabled(fAvailable);
}

void UIContextMenuNavigationAction::setForwardAvailable(bool fAvailable)
{
    if (m_pForwardButton)
        m_pForwardButton->setEnabled(fAvailable);
}

void UIContextMenuNavigationAction::prepare()
{
    QWidget *pWidget = new QWidget;
    setDefaultWidget(pWidget);
    QHBoxLayout *pMainLayout = new QHBoxLayout(pWidget);
    AssertReturnVoid(pMainLayout);

    m_pBackwardButton = new QIToolButton;
    m_pForwardButton = new QIToolButton;
    m_pHomeButton = new QIToolButton;
    m_pAddBookmarkButton = new QIToolButton;

    AssertReturnVoid(m_pBackwardButton &&
                     m_pForwardButton &&
                     m_pHomeButton);
    m_pForwardButton->setEnabled(false);
    m_pBackwardButton->setEnabled(false);
    m_pHomeButton->setIcon(UIIconPool::iconSet(":/help_browser_home_32px.png"));
    m_pForwardButton->setIcon(UIIconPool::iconSet(":/help_browser_forward_32px.png", ":/help_browser_forward_disabled_32px.png"));
    m_pBackwardButton->setIcon(UIIconPool::iconSet(":/help_browser_backward_32px.png", ":/help_browser_backward_disabled_32px.png"));
    m_pAddBookmarkButton->setIcon(UIIconPool::iconSet(":/help_browser_add_bookmark.png"));

    pMainLayout->addWidget(m_pBackwardButton);
    pMainLayout->addWidget(m_pForwardButton);
    pMainLayout->addWidget(m_pHomeButton);
    pMainLayout->addWidget(m_pAddBookmarkButton);
    pMainLayout->setContentsMargins(0, 0, 0, 0);
    //pMainLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Fixed));

    connect(m_pBackwardButton, &QIToolButton::pressed,
            this, &UIContextMenuNavigationAction::sigGoBackward);
    connect(m_pForwardButton, &QIToolButton::pressed,
            this, &UIContextMenuNavigationAction::sigGoForward);
    connect(m_pHomeButton, &QIToolButton::pressed,
            this, &UIContextMenuNavigationAction::sigGoHome);
    connect(m_pAddBookmarkButton, &QIToolButton::pressed,
            this, &UIContextMenuNavigationAction::sigAddBookmark);
}


/*********************************************************************************************************************************
*   UIFindInPageWidget implementation.                                                                                           *
*********************************************************************************************************************************/
UIFindInPageWidget::UIFindInPageWidget(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pSearchLineEdit(0)
    , m_pNextButton(0)
    , m_pPreviousButton(0)
    , m_pCloseButton(0)
    , m_previousMousePosition(-1, -1)
{
    prepare();
}

void UIFindInPageWidget::setMatchCountAndCurrentIndex(int iTotalMatchCount, int iCurrentlyScrolledIndex)
{
    if (!m_pSearchLineEdit)
        return;
    m_pSearchLineEdit->setMatchCount(iTotalMatchCount);
    m_pSearchLineEdit->setScrollToIndex(iCurrentlyScrolledIndex);
}

void UIFindInPageWidget::clearSearchField()
{
    if (!m_pSearchLineEdit)
        return;
    m_pSearchLineEdit->blockSignals(true);
    m_pSearchLineEdit->reset();
    m_pSearchLineEdit->blockSignals(false);
}

bool UIFindInPageWidget::eventFilter(QObject *pObject, QEvent *pEvent)
{
    if (pObject == m_pDragMoveLabel)
    {
        if (pEvent->type() == QEvent::Enter)
            m_pDragMoveLabel->setCursor(Qt::CrossCursor);
        else if (pEvent->type() == QEvent::Leave)
        {
            if (parentWidget())
                m_pDragMoveLabel->setCursor(parentWidget()->cursor());
        }
        else if (pEvent->type() == QEvent::MouseMove)
        {
            QMouseEvent *pMouseEvent = static_cast<QMouseEvent*>(pEvent);
            if (pMouseEvent->buttons() == Qt::LeftButton)
            {
                if (m_previousMousePosition != QPoint(-1, -1))
                    emit sigDragging(pMouseEvent->globalPos() - m_previousMousePosition);
                m_previousMousePosition = pMouseEvent->globalPos();
                m_pDragMoveLabel->setCursor(Qt::ClosedHandCursor);
            }
        }
        else if (pEvent->type() == QEvent::MouseButtonRelease)
        {
            m_previousMousePosition = QPoint(-1, -1);
            m_pDragMoveLabel->setCursor(Qt::CrossCursor);
        }
    }
    return QIWithRetranslateUI<QWidget>::eventFilter(pObject, pEvent);
}

void UIFindInPageWidget::prepare()
{
    setAutoFillBackground(true);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);

    QHBoxLayout *pLayout = new QHBoxLayout(this);
    m_pSearchLineEdit = new UISearchLineEdit;
    AssertReturnVoid(pLayout && m_pSearchLineEdit);
    setFocusProxy(m_pSearchLineEdit);
    QFontMetrics fontMetric(m_pSearchLineEdit->font());
    setMinimumSize(40 * fontMetric.width("x"),
                   fontMetric.height() +
                   qApp->style()->pixelMetric(QStyle::PM_LayoutBottomMargin) +
                   qApp->style()->pixelMetric(QStyle::PM_LayoutTopMargin));

    connect(m_pSearchLineEdit, &UISearchLineEdit::textChanged,
            this, &UIFindInPageWidget::sigSearchTextChanged);

    m_pDragMoveLabel = new QLabel;
    AssertReturnVoid(m_pDragMoveLabel);
    m_pDragMoveLabel->installEventFilter(this);
    m_pDragMoveLabel->setPixmap(QPixmap(":/drag_move_16px.png"));
    pLayout->addWidget(m_pDragMoveLabel);


    pLayout->setSpacing(0);
    pLayout->addWidget(m_pSearchLineEdit);

    m_pPreviousButton = new QIToolButton;
    m_pNextButton = new QIToolButton;
    m_pCloseButton = new QIToolButton;

    pLayout->addWidget(m_pPreviousButton);
    pLayout->addWidget(m_pNextButton);
    pLayout->addWidget(m_pCloseButton);

    m_pPreviousButton->setIcon(UIIconPool::iconSet(":/arrow_up_10px.png"));
    m_pNextButton->setIcon(UIIconPool::iconSet(":/arrow_down_10px.png"));
    m_pCloseButton->setIcon(UIIconPool::iconSet(":/close_16px.png"));

    connect(m_pPreviousButton, &QIToolButton::pressed, this, &UIFindInPageWidget::sigSelectPreviousMatch);
    connect(m_pNextButton, &QIToolButton::pressed, this, &UIFindInPageWidget::sigSelectNextMatch);
    connect(m_pCloseButton, &QIToolButton::pressed, this, &UIFindInPageWidget::sigClose);
}

void UIFindInPageWidget::retranslateUi()
{
}


/*********************************************************************************************************************************
*   UIHelpViewer implementation.                                                                                          *
*********************************************************************************************************************************/

UIHelpViewer::UIHelpViewer(const QHelpEngine *pHelpEngine, QWidget *pParent /* = 0 */)
    :QIWithRetranslateUI<QTextBrowser>(pParent)
    , m_pHelpEngine(pHelpEngine)
    , m_pFindInPageWidget(new UIFindInPageWidget(this))
    , m_fFindWidgetDragged(false)
    , m_iMarginForFindWidget(qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin))
    , m_iSelectedMatchIndex(0)
    , m_iSearchTermLength(0)
{
    m_iInitialFontPointSize = font().pointSize();
    setUndoRedoEnabled(true);
    connect(m_pFindInPageWidget, &UIFindInPageWidget::sigDragging,
            this, &UIHelpViewer::sltHandleFindWidgetDrag);
    connect(m_pFindInPageWidget, &UIFindInPageWidget::sigSearchTextChanged,
            this, &UIHelpViewer::sltHandleFindInPageSearchTextChange);

    connect(m_pFindInPageWidget, &UIFindInPageWidget::sigSelectPreviousMatch,
            this, &UIHelpViewer::sltSelectPreviousMatch);
    connect(m_pFindInPageWidget, &UIFindInPageWidget::sigSelectNextMatch,
            this, &UIHelpViewer::sltSelectNextMatch);
    connect(m_pFindInPageWidget, &UIFindInPageWidget::sigClose,
            this, &UIHelpViewer::sigCloseFindInPageWidget);

    m_pFindInPageWidget->setVisible(false);
    retranslateUi();
}

QVariant UIHelpViewer::loadResource(int type, const QUrl &name)
{
    if (name.scheme() == "qthelp" && m_pHelpEngine)
        return QVariant(m_pHelpEngine->fileData(name));
    else
        return QTextBrowser::loadResource(type, name);
}

void UIHelpViewer::emitHistoryChangedSignal()
{
    emit historyChanged();
    emit backwardAvailable(true);
}

void UIHelpViewer::setSource(const QUrl &url)
{
    QTextBrowser::setSource(url);
    QTextDocument *pDocument = document();
    if (!pDocument || pDocument->isEmpty())
    {
        setText(tr("<div><p><h3>404. Not found.</h3>The page <b>%1</b> could not be found.</p></div>").arg(url.toString()));
    }
    if (m_pFindInPageWidget && m_pFindInPageWidget->isVisible())
    {
        document()->undo();
        m_pFindInPageWidget->clearSearchField();
    }
}

void UIHelpViewer::toggleFindInPageWidget(bool fVisible)
{
    if (!m_pFindInPageWidget)
        return;
    /* Try to position the widget somewhere meaningful initially: */
    if (!m_fFindWidgetDragged)
        m_pFindInPageWidget->move(width() - m_iMarginForFindWidget - m_pFindInPageWidget->width(),
                                  m_iMarginForFindWidget);

    m_pFindInPageWidget->setVisible(fVisible);

    if (!fVisible)
    {
        document()->undo();
        m_pFindInPageWidget->clearSearchField();
    }
    else
        m_pFindInPageWidget->setFocus();
}

int UIHelpViewer::initialFontPointSize() const
{
    return m_iInitialFontPointSize;
}

void UIHelpViewer::setFont(const QFont &font)
{
    QIWithRetranslateUI<QTextBrowser>::setFont(font);
    /* Make sure the font size of the find in widget stays constant: */
    if (m_pFindInPageWidget)
    {
        QFont wFont(font);
        wFont.setPointSize(m_iInitialFontPointSize);
        m_pFindInPageWidget->setFont(wFont);
    }
}

void UIHelpViewer::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu pMenu;

    UIContextMenuNavigationAction *pNavigationActions = new UIContextMenuNavigationAction;
    pNavigationActions->setBackwardAvailable(isBackwardAvailable());
    pNavigationActions->setForwardAvailable(isForwardAvailable());

    connect(pNavigationActions, &UIContextMenuNavigationAction::sigGoBackward,
            this, &UIHelpViewer::sigGoBackward);
    connect(pNavigationActions, &UIContextMenuNavigationAction::sigGoForward,
            this, &UIHelpViewer::sigGoForward);
    connect(pNavigationActions, &UIContextMenuNavigationAction::sigGoHome,
            this, &UIHelpViewer::sigGoHome);
    connect(pNavigationActions, &UIContextMenuNavigationAction::sigAddBookmark,
            this, &UIHelpViewer::sigAddBookmark);

    QAction *pOpenLinkAction = new QAction(UIHelpBrowserWidget::tr("Open Link"));
    connect(pOpenLinkAction, &QAction::triggered,
            this, &UIHelpViewer::sltHandleOpenLink);

    QAction *pOpenInNewTabAction = new QAction(UIHelpBrowserWidget::tr("Open Link in New Tab"));
    connect(pOpenInNewTabAction, &QAction::triggered,
            this, &UIHelpViewer::sltHandleOpenLinkInNewTab);

    QAction *pCopyLink = new QAction(UIHelpBrowserWidget::tr("Copy Link"));
    connect(pCopyLink, &QAction::triggered,
            this, &UIHelpViewer::sltHandleCopyLink);

    pMenu.addAction(pNavigationActions);
    pMenu.addAction(pOpenLinkAction);
    pMenu.addAction(pOpenInNewTabAction);
    pMenu.addAction(pCopyLink);

    QString strAnchor = anchorAt(event->pos());
    if (!strAnchor.isEmpty())
    {
        QString strLink = source().resolved(anchorAt(event->pos())).toString();
        pOpenLinkAction->setData(strLink);
        pOpenInNewTabAction->setData(strLink);
        pCopyLink->setData(strLink);
    }
    else
    {
        pOpenLinkAction->setEnabled(false);
        pOpenInNewTabAction->setEnabled(false);
        pCopyLink->setEnabled(false);
    }
    pMenu.exec(event->globalPos());
}

void UIHelpViewer::resizeEvent(QResizeEvent *pEvent)
{
    /* Make sure the widget stays inside the parent during parent resize: */
    if (m_pFindInPageWidget)
    {
        if (!isRectInside(m_pFindInPageWidget->geometry(), m_iMarginForFindWidget))
            moveFindWidgetIn(m_iMarginForFindWidget);
    }
    QIWithRetranslateUI<QTextBrowser>::resizeEvent(pEvent);
}

void UIHelpViewer::wheelEvent(QWheelEvent *pEvent)
{
    int iPreviousSize = font().pointSize();
    QTextBrowser::wheelEvent(pEvent);
    /* Don't allow font size to get too large or small: */
    if (font().pointSize() >= UIHelpBrowserWidget::fontScaleMinMax.second * m_iInitialFontPointSize ||
        font().pointSize() <= UIHelpBrowserWidget::fontScaleMinMax.first * m_iInitialFontPointSize)
    {
        QFont mFont = font();
        mFont.setPointSize(iPreviousSize);
        setFont(mFont);
    }
    else
        emit sigFontPointSizeChanged(font().pointSize());
}

void UIHelpViewer::retranslateUi()
{
}

void UIHelpViewer::moveFindWidgetIn(int iMargin)
{
    if (!m_pFindInPageWidget)
        return;

    QRect  rect = m_pFindInPageWidget->geometry();
    if (rect.left() < iMargin)
        rect.translate(-rect.left() + iMargin, 0);
    if (rect.right() > width() - iMargin)
        rect.translate((width() - iMargin - rect.right()), 0);
    if (rect.top() < iMargin)
        rect.translate(0, -rect.top() + iMargin);

    if (rect.bottom() > height() - iMargin)
        rect.translate(0, (height() - iMargin - rect.bottom()));
    m_pFindInPageWidget->setGeometry(rect);
    m_pFindInPageWidget->update();
}

bool UIHelpViewer::isRectInside(const QRect &rect, int iMargin) const
{
    if (rect.left() < iMargin || rect.top() < iMargin)
        return false;
    if (rect.right() > width() - iMargin || rect.bottom() > height() - iMargin)
        return false;
    return true;
}

void UIHelpViewer::findAllMatches(const QString &searchString)
{
    QTextDocument *pDocument = document();
    AssertReturnVoid(pDocument);

    m_matchedCursorPosition.clear();
    if (searchString.isEmpty())
        return;
    QTextCursor cursor(pDocument);
    QTextDocument::FindFlags flags;
    int iMatchCount = 0;
    while (!cursor.isNull() && !cursor.atEnd())
    {
        cursor = pDocument->find(searchString, cursor, flags);
        if (!cursor.isNull())
        {
            m_matchedCursorPosition << cursor.position() - searchString.length();
            ++iMatchCount;
        }
    }
}

void UIHelpViewer::highlightFinds(int iSearchTermLength)
{
    QTextDocument* pDocument = document();
    AssertReturnVoid(pDocument);
    /* Clear previous highlight: */
    pDocument->undo();

    QTextCursor highlightCursor(pDocument);
    QTextCursor cursor(pDocument);
    cursor.beginEditBlock();
    for (int i = 0; i < m_matchedCursorPosition.size(); ++i)
    {
        highlightCursor.setPosition(m_matchedCursorPosition[i]);

        QTextCharFormat colorFormat(highlightCursor.charFormat());
        colorFormat.setBackground(Qt::yellow);

        highlightCursor.setPosition(m_matchedCursorPosition[i] + iSearchTermLength, QTextCursor::KeepAnchor);
        if (!highlightCursor.isNull())
            highlightCursor.setCharFormat(colorFormat);
    }
    cursor.endEditBlock();
}

void UIHelpViewer::selectMatch(int iMatchIndex, int iSearchStringLength)
{
    QTextCursor cursor = textCursor();
    /* Move the cursor to the beginning of the matched string: */
    cursor.setPosition(m_matchedCursorPosition.at(iMatchIndex), QTextCursor::MoveAnchor);
    /* Move the cursor to the end of the matched string while keeping the anchor at the begining thus selecting the text: */
    cursor.setPosition(m_matchedCursorPosition.at(iMatchIndex) + iSearchStringLength, QTextCursor::KeepAnchor);
    ensureCursorVisible();
    setTextCursor(cursor);
}

void UIHelpViewer::sltHandleOpenLinkInNewTab()
{
    QAction *pSender = qobject_cast<QAction*>(sender());
    if (!pSender)
        return;
    QUrl url = pSender->data().toUrl();
    if (url.isValid())
        emit sigOpenLinkInNewTab(url);
}

void UIHelpViewer::sltHandleOpenLink()
{
    QAction *pSender = qobject_cast<QAction*>(sender());
    if (!pSender)
        return;
    QUrl url = pSender->data().toUrl();
    if (url.isValid())
        QTextBrowser::setSource(url);
}

void UIHelpViewer::sltHandleCopyLink()
{
    QAction *pSender = qobject_cast<QAction*>(sender());
    if (!pSender)
        return;
    QUrl url = pSender->data().toUrl();
    if (url.isValid())
    {
        QClipboard *pClipboard = QApplication::clipboard();
        if (pClipboard)
            pClipboard->setText(url.toString());
    }
}

void UIHelpViewer::sltHandleFindWidgetDrag(const QPoint &delta)
{
    if (!m_pFindInPageWidget)
        return;
    QRect geo = m_pFindInPageWidget->geometry();
    geo.translate(delta);

    /* Allow the move if m_pFindInPageWidget stays inside after the move: */
    if (isRectInside(geo, m_iMarginForFindWidget))
        m_pFindInPageWidget->move(m_pFindInPageWidget->pos() + delta);
    m_fFindWidgetDragged = true;
    update();
}

void UIHelpViewer::sltHandleFindInPageSearchTextChange(const QString &strSearchText)
{
    m_iSearchTermLength = strSearchText.length();
    findAllMatches(strSearchText);
    highlightFinds(m_iSearchTermLength);
    //scrollToMatch(int iMatchIndex);
    selectMatch(0, m_iSearchTermLength);
    if (m_pFindInPageWidget)
        m_pFindInPageWidget->setMatchCountAndCurrentIndex(m_matchedCursorPosition.size(), 0);
}

void UIHelpViewer::sltSelectPreviousMatch()
{
    m_iSelectedMatchIndex = m_iSelectedMatchIndex <= 0 ? m_matchedCursorPosition.size() - 1 : (m_iSelectedMatchIndex - 1);
    selectMatch(m_iSelectedMatchIndex, m_iSearchTermLength);
    if (m_pFindInPageWidget)
        m_pFindInPageWidget->setMatchCountAndCurrentIndex(m_matchedCursorPosition.size(), m_iSelectedMatchIndex);
}

void UIHelpViewer::sltSelectNextMatch()
{
    m_iSelectedMatchIndex = m_iSelectedMatchIndex >= m_matchedCursorPosition.size() - 1 ? 0 : (m_iSelectedMatchIndex + 1);
    selectMatch(m_iSelectedMatchIndex, m_iSearchTermLength);
    if (m_pFindInPageWidget)
        m_pFindInPageWidget->setMatchCountAndCurrentIndex(m_matchedCursorPosition.size(), m_iSelectedMatchIndex);
}

#include "UIHelpViewer.moc"

#endif /*#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP) && (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))*/
