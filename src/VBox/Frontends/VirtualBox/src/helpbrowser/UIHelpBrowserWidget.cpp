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
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFont>
#include <QHBoxLayout>
#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP) && (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))
 #include <QtHelp/QHelpEngine>
 #include <QtHelp/QHelpContentWidget>
 #include <QtHelp/QHelpIndexWidget>
 #include <QtHelp/QHelpSearchEngine>
 #include <QtHelp/QHelpSearchQueryWidget>
 #include <QtHelp/QHelpSearchResultWidget>
#endif
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QtPrintSupport/QPrintDialog>
#include <QtPrintSupport/QPrinter>
#include <QPushButton>
#include <QSpacerItem>
#include <QSplitter>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QWidgetAction>
#ifdef RT_OS_SOLARIS
# include <QFontDatabase>
#endif

/* GUI includes: */
#include "UICommon.h"
#include "QIAdvancedSlider.h"
#include "QIFileDialog.h"
#include "QITabWidget.h"
#include "QIToolBar.h"
#include "QIToolButton.h"
#include "UIActionPool.h"
#include "UIExtraDataManager.h"
#include "UIHelpBrowserWidget.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UISearchLineEdit.h"


/* COM includes: */
#include "CSystemProperties.h"

#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP) && (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))

enum HelpBrowserTabs
{
    HelpBrowserTabs_TOC = 0,
    HelpBrowserTabs_Search,
    HelpBrowserTabs_Bookmarks,
    HelpBrowserTabs_Index,
    HelpBrowserTabs_Max
};
Q_DECLARE_METATYPE(HelpBrowserTabs);

static const int iBookmarkUrlDataType = 6;
static const QPair<float, float> fontScaleMinMax(0.5f, 3.f);
static int iFontPointSizeChangeStep = 2;


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
*   UIFontScaleWidget definition.                                                                                         *
*********************************************************************************************************************************/
class UIFontScaleWidget : public QIWithRetranslateUI<QWidget>
{

    Q_OBJECT;

signals:

    void sigFontPointSizeChanged(int iNewFontPointSize);

public:

    UIFontScaleWidget(int iInitialFontPointSize, QWidget *pParent = 0);
    void setFontPointSize(int iFontPointSize);
    int fontPointSize() const;

protected:

    void retranslateUi() /* override */;

private slots:

    void sltSetFontPointSize();

private:

    void prepare();
    int fontPercentage() const;
    QHBoxLayout  *m_pMainLayout;
    QIToolButton *m_pMinusButton;
    QIToolButton *m_pResetButton;
    QIToolButton *m_pPlusButton;
    QIAdvancedSlider *m_pSlider;
    QLabel *m_pValueLabel;
    int m_iInitialFontPointSize;
    int m_iFontPointSize;
};


/*********************************************************************************************************************************
*   UIBookmarksListWidget definition.                                                                                            *
*********************************************************************************************************************************/
class UIBookmarksListWidget : public QListWidget
{

    Q_OBJECT;

signals:

    void sigBookmarkDoubleClick(const QUrl &url);

public:

    UIBookmarksListWidget(QWidget *pParent = 0);

protected:

    void mouseDoubleClickEvent(QMouseEvent *event) /* override */;
    void mousePressEvent(QMouseEvent *pEvent) /* override */;
};


/*********************************************************************************************************************************
*   UIBookmarksListContainer definition.                                                                                         *
*********************************************************************************************************************************/
class UIBookmarksListContainer : public QIWithRetranslateUI<QWidget>
{

    Q_OBJECT;

signals:

    void sigBookmarkDoubleClick(const QUrl &url);
    void sigListWidgetContextMenuRequest(const QPoint &listWidgetLocalPos);

public:

    UIBookmarksListContainer(QWidget *pParent = 0);
    void addBookmark(const QUrl &url, const QString &strTitle);
    /** Return all bookmarks a url, title pair list. */
    QStringList bookmarks() const;
    QUrl currentBookmarkUrl();

public:

    void sltDeleteSelectedBookmark();

protected:

    void retranslateUi() /* override */;

private slots:

    void sltHandleContextMenuRequest(const QPoint &listWidgetLocalPos);

private:

    void prepare();
    int itemIndex(const QUrl &url);

    QVBoxLayout  *m_pMainLayout;
    UIBookmarksListWidget  *m_pListWidget;
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
*   UIHelpBrowserViewer definition.                                                                                        *
*********************************************************************************************************************************/

class UIHelpBrowserViewer : public QIWithRetranslateUI<QTextBrowser>
{
    Q_OBJECT;

signals:

    void sigOpenLinkInNewTab(const QUrl &url);
    void sigCloseFindInPageWidget();
    void sigFontPointSizeChanged(int iFontPointSize);
    void sigGoBackward();
    void sigGoForward();
    void sigGoHome();
    void sigAddBookmark();

public:

    UIHelpBrowserViewer(const QHelpEngine *pHelpEngine, QWidget *pParent = 0);
    virtual QVariant loadResource(int type, const QUrl &name) /* override */;
    void emitHistoryChangedSignal();
    void setSource(const QUrl &url) /* override */;
    void toggleFindInPageWidget(bool fVisible);
    int initialFontPointSize() const;
    void setFont(const QFont &);

public slots:


protected:

    virtual void contextMenuEvent(QContextMenuEvent *event) /* override */;
    virtual void resizeEvent(QResizeEvent *pEvent) /* override */;
    virtual void wheelEvent(QWheelEvent *pEvent) /* override */;

private slots:

    void sltHandleOpenLinkInNewTab();
    void sltHandleOpenLink();
    void sltHandleCopyLink();
    void sltHandleFindWidgetDrag(const QPoint &delta);
    void sltHandleFindInPageSearchTextChange(const QString &strSearchText);
    void sltSelectPreviousMatch();
    void sltSelectNextMatch();

private:

    void retranslateUi();
    bool isRectInside(const QRect &rect, int iMargin) const;
    void moveFindWidgetIn(int iMargin);
    void findAllMatches(const QString &searchString);
    void highlightFinds(int iSearchTermLength);
    void selectMatch(int iMatchIndex, int iSearchStringLength);
    const QHelpEngine* m_pHelpEngine;
    UIFindInPageWidget *m_pFindInPageWidget;
    /* Initilized as false and set to true once the user drag moves the find widget. */
    bool m_fFindWidgetDragged;
    const int m_iMarginForFindWidget;
    /** Document positions of the cursors within the document for all matches. */
    QVector<int>   m_matchedCursorPosition;
    int m_iSelectedMatchIndex;
    int m_iSearchTermLength;
    int m_iInitialFontPointSize;
};

/*********************************************************************************************************************************
*   UIHelpBrowserTab definition.                                                                                        *
*********************************************************************************************************************************/

class UIHelpBrowserTab : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigSourceChanged(const QUrl &url);
    void sigTitleUpdate(const QString &strTitle);
    void sigOpenLinkInNewTab(const QUrl &url);
    void sigAddBookmark(const QUrl &url, const QString &strTitle);
    void sigFontPointSizeChanged(int iFontPointSize);
    void sigLinkHighlighted(const QString &strLink);

public:

    UIHelpBrowserTab(const QHelpEngine  *pHelpEngine, const QUrl &homeUrl,
                     const QUrl &initialUrl, QWidget *pParent = 0);

    QUrl source() const;
    void setSource(const QUrl &url);
    QString documentTitle() const;
    void setToolBarVisible(bool fVisible);
    void print(QPrinter &printer);
    int initialFontPointSize() const;
    void setFontPointSize(int iPointSize);
    int fontPointSize() const;

private slots:

    void sltHandleHomeAction();
    void sltHandleForwardAction();
    void sltHandleBackwardAction();
    void sltHandleFindInPageAction(bool fToggled);
    void sltHandleHistoryChanged();
    void sltHandleAddressBarIndexChanged(int index);
    void sltHandleAddBookmarkAction();
    void sltAnchorClicked(const QUrl &link);
    void sltCloseFindInPageWidget();

private:

    void prepare(const QUrl &initialUrl);
    void prepareWidgets(const QUrl &initialUrl);
    void prepareToolBarAndAddressBar();
    virtual void retranslateUi() /* override */;
    void setActionTextAndToolTip(QAction *pAction, const QString &strText, const QString &strToolTip);

    QAction     *m_pHomeAction;
    QAction     *m_pForwardAction;
    QAction     *m_pBackwardAction;
    QAction     *m_pAddBookmarkAction;
    QAction     *m_pFindInPageAction;

    QVBoxLayout *m_pMainLayout;
    QIToolBar   *m_pToolBar;
    QComboBox   *m_pAddressBar;
    UIHelpBrowserViewer *m_pContentViewer;
    const QHelpEngine* m_pHelpEngine;
    QUrl m_homeUrl;
};


/*********************************************************************************************************************************
*   UIHelpBrowserTabManager definition.                                                                                          *
*********************************************************************************************************************************/

class UIHelpBrowserTabManager : public QITabWidget
{
    Q_OBJECT;

signals:

    void sigSourceChanged(const QUrl &url);
    void sigAddBookmark(const QUrl &url, const QString &strTitle);
    /** list.first is tab title and list.second is tab's index. */
    void sigTabsListChanged(const QStringList &titleList);
    void sigLinkHighlighted(const QString &strLink);

public:

    UIHelpBrowserTabManager(const QHelpEngine  *pHelpEngine, const QUrl &homeUrl,
                            const QStringList &urlList, QWidget *pParent = 0);
    /* Return the list of urls of all open tabs as QStringList. */
    QStringList tabUrlList() const;
    QStringList tabTitleList() const;

    /** Either start with a single tab showin the home url or saved tab(s). Depending on the params. passed to ctor. */
    void initializeTabs();
    /* Url of the current tab. */
    QUrl currentSource() const;
    void setSource(const QUrl &url, bool fNewTab = false);
    void setToolBarVisible(bool fVisible);
    void printCurrent(QPrinter &printer);
    int initialFontPointSize() const;
    void setFontPointSize(int iPointSize);
    int fontPointSize() const;
    void setFontScaleWidgetVisible(bool fToggled);
    void switchToTab(int iIndex);

public slots:

    void sltHandleCloseCurrentTab();
    void sltHandleCloseOtherTabs();

protected:

    virtual void paintEvent(QPaintEvent *pEvent) /* override */;

private slots:

    void sltHandletabTitleChange(const QString &strTitle);
    void sltHandleOpenLinkInNewTab(const QUrl &url);
    void sltHandleTabClose(int iTabIndex);
    void sltHandleContextMenuTabClose();
    void sltHandleCurrentChanged(int iTabIndex);
    void sltHandleFontSizeChange(int iFontPointSize);
    void sltShowTabBarContextMenu(const QPoint &pos);
    void sltHandleCloseOtherTabsContextMenuAction();

private:

    void prepare();
    void clearAndDeleteTabs();
    void addNewTab(const QUrl &initialUrl);
    /** Check if lists of tab url/title has changed. if so emit a signal. */
    void updateTabUrlTitleList();
    /** Closes all tabs other than the one with index @param iTabIndex. */
    void closeAllTabsBut(int iTabIndex);
    const QHelpEngine* m_pHelpEngine;
    UIFontScaleWidget *m_pFontScaleWidget;
    QUrl m_homeUrl;
    QStringList m_savedUrlList;
    /** Immediately switch the newly created tab. Otherwise open the tab in background. */
    bool m_fSwitchToNewTab;
    bool m_fToolBarVisible;
    QStringList m_tabTitleList;
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
*   UIFontScaleWidget implementation.                                                                                            *
*********************************************************************************************************************************/

UIFontScaleWidget::UIFontScaleWidget(int iInitialFontPointSize, QWidget *pParent /* = 0 */)
    :QIWithRetranslateUI<QWidget>(pParent)
    , m_pMainLayout(0)
    , m_pMinusButton(0)
    , m_pResetButton(0)
    , m_pPlusButton(0)
    , m_pSlider(0)
    , m_pValueLabel(0)
    , m_iInitialFontPointSize(iInitialFontPointSize)
    , m_iFontPointSize(iInitialFontPointSize)
{
    prepare();
}

void UIFontScaleWidget::setFontPointSize(int iFontPointSize)
{
    if (m_iFontPointSize == iFontPointSize)
        return;
    m_iFontPointSize = iFontPointSize;
    if (m_pValueLabel && m_iInitialFontPointSize != 0)
    {
        m_pValueLabel->setText(QString("%1\%").arg(QString::number(fontPercentage())));
    }
}

int UIFontScaleWidget::fontPointSize() const
{
    return m_iFontPointSize;
}

void UIFontScaleWidget::retranslateUi()
{
}

void UIFontScaleWidget::prepare()
{
    setAutoFillBackground(true);
    m_pMainLayout = new QHBoxLayout(this);
    AssertReturnVoid(m_pMainLayout);
    m_pMainLayout->setSpacing(0);
    m_pMainLayout->setContentsMargins(0, 0, 0, 0);
    m_pMinusButton = new QIToolButton;
    m_pResetButton = new QIToolButton;
    m_pPlusButton = new QIToolButton;
    //m_pSlider = new QIAdvancedSlider;
    m_pValueLabel = new QLabel;
    AssertReturnVoid(m_pMinusButton &&
                     m_pResetButton &&
                     m_pPlusButton &&
                     m_pValueLabel);

    m_pValueLabel->setText(QString("%1\%").arg(QString::number(fontPercentage())));

    m_pMinusButton->setIcon(UIIconPool::iconSet(":/help_browser_minus_32px.png"));
    m_pResetButton->setIcon(UIIconPool::iconSet(":/help_browser_reset_32px.png"));
    m_pPlusButton->setIcon(UIIconPool::iconSet(":/help_browser_plus_32px.png"));

    connect(m_pPlusButton, &QIToolButton::pressed, this, &UIFontScaleWidget::sltSetFontPointSize);
    connect(m_pMinusButton, &QIToolButton::pressed, this, &UIFontScaleWidget::sltSetFontPointSize);
    connect(m_pResetButton, &QIToolButton::pressed, this, &UIFontScaleWidget::sltSetFontPointSize);

    m_pMainLayout->addWidget(m_pResetButton);
    m_pMainLayout->addWidget(m_pMinusButton);
    //m_pMainLayout->addWidget(m_pSlider);
    m_pMainLayout->addWidget(m_pValueLabel);
    m_pMainLayout->addWidget(m_pPlusButton);
}

int UIFontScaleWidget::fontPercentage() const
{
    if (m_iInitialFontPointSize == 0)
        return 0;
    return 100 * m_iFontPointSize / m_iInitialFontPointSize;
}

void UIFontScaleWidget::sltSetFontPointSize()
{
    if (!sender())
        return;
    int iNewSize = m_iFontPointSize;
    if (sender() == m_pMinusButton)
        iNewSize -= iFontPointSizeChangeStep;
    else if (sender() == m_pPlusButton)
        iNewSize += iFontPointSizeChangeStep;
    else if (sender() == m_pResetButton)
        iNewSize = m_iInitialFontPointSize;
    if (iNewSize >= fontScaleMinMax.second * m_iInitialFontPointSize ||
        iNewSize <= fontScaleMinMax.first * m_iInitialFontPointSize)
        iNewSize = m_iFontPointSize;
    setFontPointSize(iNewSize);
    emit sigFontPointSizeChanged(m_iFontPointSize);
}


/*********************************************************************************************************************************
*   UIBookmarksListWidget implementation.                                                                                        *
*********************************************************************************************************************************/
UIBookmarksListWidget::UIBookmarksListWidget(QWidget *pParent /* = 0 */)
    :QListWidget(pParent)
{
    setSelectionMode(QAbstractItemView::SingleSelection);
}

void UIBookmarksListWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    QListWidgetItem *pItem = currentItem();
    if (!pItem)
        return;
    emit sigBookmarkDoubleClick(pItem->data(iBookmarkUrlDataType).toUrl());
    QListWidget::mouseDoubleClickEvent(event);
}

void UIBookmarksListWidget::mousePressEvent(QMouseEvent *pEvent)
{
    if (!indexAt(pEvent->pos()).isValid())
    {
        clearSelection();
        setCurrentItem(0);
    }
    QListWidget::mousePressEvent(pEvent);
}


/*********************************************************************************************************************************
*   UIBookmarksListContainer implementation.                                                                                     *
*********************************************************************************************************************************/

UIBookmarksListContainer::UIBookmarksListContainer(QWidget *pParent /* = 0 */)
    :QIWithRetranslateUI<QWidget>(pParent)
    , m_pMainLayout(0)
    , m_pListWidget(0)
{
    prepare();
}

void UIBookmarksListContainer::addBookmark(const QUrl &url, const QString &strTitle)
{
    if (!m_pListWidget)
        return;
    if (itemIndex(url) != -1)
        return;
    QListWidgetItem *pNewItem = new QListWidgetItem(strTitle, m_pListWidget);
    pNewItem->setData(iBookmarkUrlDataType, url);
    pNewItem->setToolTip(url.toString());
}

QStringList UIBookmarksListContainer::bookmarks() const
{
    if (!m_pListWidget)
        return QStringList();
    QStringList bookmarks;
    for (int i = 0; i < m_pListWidget->count(); ++i)
    {
        QListWidgetItem *pItem = m_pListWidget->item(i);
        if (!pItem)
            continue;
        bookmarks << pItem->data(iBookmarkUrlDataType).toUrl().toString() << pItem->text();
    }
    return bookmarks;
}

QUrl UIBookmarksListContainer::currentBookmarkUrl()
{
    if (!m_pListWidget || !m_pListWidget->currentItem())
        return QUrl();
    return m_pListWidget->currentItem()->data(iBookmarkUrlDataType).toUrl();
}

void UIBookmarksListContainer::sltDeleteSelectedBookmark()
{
    if (!m_pListWidget || !m_pListWidget->currentItem())
        return;
    QListWidgetItem *pCurrentItem = m_pListWidget->takeItem(m_pListWidget->currentRow());

    delete pCurrentItem;


}

void UIBookmarksListContainer::retranslateUi()
{
}

void UIBookmarksListContainer::prepare()
{
    m_pMainLayout = new QVBoxLayout(this);
    AssertReturnVoid(m_pMainLayout);
    m_pMainLayout->setContentsMargins(0, 0, 0, 0);

    m_pListWidget = new UIBookmarksListWidget;
    AssertReturnVoid(m_pListWidget);
    m_pMainLayout->addWidget(m_pListWidget);
    m_pListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_pListWidget, &UIBookmarksListWidget::sigBookmarkDoubleClick,
            this, &UIBookmarksListContainer::sigBookmarkDoubleClick);
    connect(m_pListWidget, &UIBookmarksListWidget::customContextMenuRequested,
            this, &UIBookmarksListContainer::sltHandleContextMenuRequest);
}

void UIBookmarksListContainer::sltHandleContextMenuRequest(const QPoint &listWidgetLocalPos)
{
    if (!m_pListWidget || !m_pListWidget->currentItem())
        return;
    emit sigListWidgetContextMenuRequest(listWidgetLocalPos);
}

int UIBookmarksListContainer::itemIndex(const QUrl &url)
{
    if (!m_pListWidget || !url.isValid())
        return -1;
    for (int i = 0; i < m_pListWidget->count(); ++i)
    {
        if (m_pListWidget->item(i)->data(iBookmarkUrlDataType).toUrl() == url)
            return i;
    }
    return -1;
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
*   UIHelpBrowserTab implementation.                                                                                        *
*********************************************************************************************************************************/

UIHelpBrowserTab::UIHelpBrowserTab(const QHelpEngine  *pHelpEngine, const QUrl &homeUrl,
                                   const QUrl &initialUrl, QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pHomeAction(0)
    , m_pForwardAction(0)
    , m_pBackwardAction(0)
    , m_pAddBookmarkAction(0)
    , m_pFindInPageAction(0)
    , m_pMainLayout(0)
    , m_pToolBar(0)
    , m_pAddressBar(0)
    , m_pContentViewer(0)
    , m_pHelpEngine(pHelpEngine)
    , m_homeUrl(homeUrl)
{
    if (initialUrl.isValid())
        prepare(initialUrl);
    else
        prepare(m_homeUrl);
}

QUrl UIHelpBrowserTab::source() const
{
    if (!m_pContentViewer)
        return QUrl();
    return m_pContentViewer->source();
}

void UIHelpBrowserTab::setSource(const QUrl &url)
{
    if (m_pContentViewer)
    {
        m_pContentViewer->blockSignals(true);
        m_pContentViewer->setSource(url);
        m_pContentViewer->blockSignals(false);
        /* emit historyChanged signal explicitly since we have blocked the signals: */
        m_pContentViewer->emitHistoryChangedSignal();
    }
}

QString UIHelpBrowserTab::documentTitle() const
{
    if (!m_pContentViewer)
        return QString();
    return m_pContentViewer->documentTitle();
}

void UIHelpBrowserTab::setToolBarVisible(bool fVisible)
{
    if (m_pToolBar)
        m_pToolBar->setVisible(fVisible);
    if (m_pAddressBar)
        m_pAddressBar->setVisible(fVisible);
}

void UIHelpBrowserTab::print(QPrinter &printer)
{
    if (m_pContentViewer)
        m_pContentViewer->print(&printer);
}

int UIHelpBrowserTab::initialFontPointSize() const
{
    if (m_pContentViewer)
        return m_pContentViewer->initialFontPointSize();
    return 0;
}

void UIHelpBrowserTab::setFontPointSize(int iPointSize)
{
    if (m_pContentViewer)
    {
        if (m_pContentViewer->font().pointSize() == iPointSize)
            return;
        QFont mFont = m_pContentViewer->font();
        mFont.setPointSize(iPointSize);
        m_pContentViewer->setFont(mFont);
    }
}

int UIHelpBrowserTab::fontPointSize() const
{
    if (!m_pContentViewer)
        return 0;
    return m_pContentViewer->font().pointSize();
}

void UIHelpBrowserTab::prepare(const QUrl &initialUrl)
{
    m_pMainLayout = new QVBoxLayout(this);
    AssertReturnVoid(m_pMainLayout);
    prepareToolBarAndAddressBar();
    prepareWidgets(initialUrl);
    retranslateUi();
}

void UIHelpBrowserTab::prepareWidgets(const QUrl &initialUrl)
{
    m_pContentViewer = new UIHelpBrowserViewer(m_pHelpEngine);
    AssertReturnVoid(m_pContentViewer);
    m_pMainLayout->setContentsMargins(0, 0, 0, 0);
    m_pMainLayout->setSpacing(0);

    m_pMainLayout->addWidget(m_pContentViewer);
    m_pContentViewer->setOpenExternalLinks(false);
    connect(m_pContentViewer, &UIHelpBrowserViewer::sourceChanged,
        this, &UIHelpBrowserTab::sigSourceChanged);
    connect(m_pContentViewer, &UIHelpBrowserViewer::historyChanged,
        this, &UIHelpBrowserTab::sltHandleHistoryChanged);
    connect(m_pContentViewer, &UIHelpBrowserViewer::anchorClicked,
        this, &UIHelpBrowserTab::sltAnchorClicked);
    connect(m_pContentViewer, &UIHelpBrowserViewer::sigOpenLinkInNewTab,
        this, &UIHelpBrowserTab::sigOpenLinkInNewTab);
    connect(m_pContentViewer, &UIHelpBrowserViewer::sigCloseFindInPageWidget,
            this, &UIHelpBrowserTab::sltCloseFindInPageWidget);
    connect(m_pContentViewer, &UIHelpBrowserViewer::sigFontPointSizeChanged,
            this, &UIHelpBrowserTab::sigFontPointSizeChanged);
    connect(m_pContentViewer, &UIHelpBrowserViewer::sigGoBackward,
            this, &UIHelpBrowserTab::sltHandleBackwardAction);
    connect(m_pContentViewer, &UIHelpBrowserViewer::sigGoForward,
            this, &UIHelpBrowserTab::sltHandleForwardAction);
    connect(m_pContentViewer, &UIHelpBrowserViewer::sigGoHome,
            this, &UIHelpBrowserTab::sltHandleHomeAction);
    connect(m_pContentViewer, &UIHelpBrowserViewer::sigAddBookmark,
            this, &UIHelpBrowserTab::sltHandleAddBookmarkAction);
    connect(m_pContentViewer, static_cast<void(UIHelpBrowserViewer::*)(const QString&)>(&UIHelpBrowserViewer::highlighted),
            this, &UIHelpBrowserTab::sigLinkHighlighted);

    m_pContentViewer->setSource(initialUrl);
}

void UIHelpBrowserTab::prepareToolBarAndAddressBar()
{
    m_pHomeAction =
        new QAction(UIIconPool::iconSet(":/help_browser_home_32px.png"), QString(), this);
    m_pForwardAction =
        new QAction(UIIconPool::iconSet(":/help_browser_forward_32px.png", ":/help_browser_forward_disabled_32px.png"), QString(), this);
    m_pBackwardAction =
        new QAction(UIIconPool::iconSet(":/help_browser_backward_32px.png", ":/help_browser_backward_disabled_32px.png"), QString(), this);
    m_pAddBookmarkAction =
        new QAction(UIIconPool::iconSet(":/help_browser_add_bookmark.png"), QString(), this);
    m_pFindInPageAction =
        new QAction(UIIconPool::iconSet(":/help_browser_search.png"), QString(), this);

    AssertReturnVoid(m_pHomeAction && m_pForwardAction &&
                     m_pBackwardAction && m_pAddBookmarkAction &&
                     m_pFindInPageAction);
    m_pFindInPageAction->setCheckable(true);

    connect(m_pHomeAction, &QAction::triggered, this, &UIHelpBrowserTab::sltHandleHomeAction);
    connect(m_pAddBookmarkAction, &QAction::triggered, this, &UIHelpBrowserTab::sltHandleAddBookmarkAction);
    connect(m_pForwardAction, &QAction::triggered, this, &UIHelpBrowserTab::sltHandleForwardAction);
    connect(m_pBackwardAction, &QAction::triggered, this, &UIHelpBrowserTab::sltHandleBackwardAction);
    connect(m_pFindInPageAction, &QAction::toggled, this, &UIHelpBrowserTab::sltHandleFindInPageAction);

    m_pForwardAction->setEnabled(false);
    m_pBackwardAction->setEnabled(false);

    m_pToolBar = new QIToolBar;
    AssertReturnVoid(m_pToolBar);
    m_pToolBar->addAction(m_pBackwardAction);
    m_pToolBar->addAction(m_pForwardAction);
    m_pToolBar->addAction(m_pHomeAction);
    m_pToolBar->addAction(m_pAddBookmarkAction);
    m_pToolBar->addAction(m_pFindInPageAction);

    m_pAddressBar = new QComboBox();
    m_pAddressBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_pAddressBar, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIHelpBrowserTab::sltHandleAddressBarIndexChanged);


    QHBoxLayout *pTopLayout = new QHBoxLayout;
    pTopLayout->addWidget(m_pToolBar);
    pTopLayout->addWidget(m_pAddressBar);
    m_pMainLayout->addLayout(pTopLayout);
}

void UIHelpBrowserTab::setActionTextAndToolTip(QAction *pAction, const QString &strText, const QString &strToolTip)
{
    if (!pAction)
        return;
    pAction->setText(strText);
    pAction->setToolTip(strToolTip);
}

void UIHelpBrowserTab::retranslateUi()
{
    setActionTextAndToolTip(m_pHomeAction, tr("Home"), tr("Return to Start Page"));
    setActionTextAndToolTip(m_pBackwardAction, tr("Backward"), tr("Navigate to Previous Page"));
    setActionTextAndToolTip(m_pForwardAction, tr("Forward"), tr("Navigate to Next Page"));
    setActionTextAndToolTip(m_pAddBookmarkAction, tr("Bookmark"), tr("Add a New Bookmark"));
    setActionTextAndToolTip(m_pFindInPageAction, tr("Find in Page"), tr("Find a String in the Current Page"));
}

void UIHelpBrowserTab::sltHandleHomeAction()
{
    if (!m_pContentViewer)
        return;
    m_pContentViewer->setSource(m_homeUrl);
}

void UIHelpBrowserTab::sltHandleForwardAction()
{
    if (m_pContentViewer)
        m_pContentViewer->forward();
}

void UIHelpBrowserTab::sltHandleBackwardAction()
{
    if (m_pContentViewer)
        m_pContentViewer->backward();
}

void UIHelpBrowserTab::sltHandleFindInPageAction(bool fToggled)
{
    if (m_pContentViewer)
        m_pContentViewer->toggleFindInPageWidget(fToggled);
}

void UIHelpBrowserTab::sltHandleHistoryChanged()
{
    if (!m_pContentViewer)
        return;
    int iCurrentIndex = 0;
    /* QTextBrower history has negative and positive indices for bacward and forward items, respectively.
     * 0 is the current item: */
    m_pAddressBar->blockSignals(true);
    m_pAddressBar->clear();
    for (int i = -1 * m_pContentViewer->backwardHistoryCount(); i <= m_pContentViewer->forwardHistoryCount(); ++i)
    {
        int iIndex = m_pAddressBar->count();
        m_pAddressBar->addItem(m_pContentViewer->historyUrl(i).toString(), i);
        m_pAddressBar->setItemData(iIndex, m_pContentViewer->historyTitle(i), Qt::ToolTipRole);
        if (i == 0)
            iCurrentIndex = m_pAddressBar->count();
    }
    /* Make sure address bar show the current item: */
    m_pAddressBar->setCurrentIndex(iCurrentIndex - 1);
    m_pAddressBar->blockSignals(false);

    if (m_pBackwardAction)
        m_pBackwardAction->setEnabled(m_pContentViewer->isBackwardAvailable());
    if (m_pForwardAction)
        m_pForwardAction->setEnabled(m_pContentViewer->isForwardAvailable());

    emit sigTitleUpdate(m_pContentViewer->historyTitle(0));
}

void UIHelpBrowserTab::sltHandleAddressBarIndexChanged(int iIndex)
{
    if (!m_pAddressBar && iIndex >= m_pAddressBar->count())
        return;
    int iHistoryIndex = m_pAddressBar->itemData(iIndex).toInt();
    /* There seems to be no way to one-step-jump to a history item: */
    if (iHistoryIndex == 0)
        return;
    else if (iHistoryIndex > 0)
        for (int i = 0; i < iHistoryIndex; ++i)
            m_pContentViewer->forward();
    else
        for (int i = 0; i > iHistoryIndex ; --i)
            m_pContentViewer->backward();
}

void UIHelpBrowserTab::sltHandleAddBookmarkAction()
{
    emit sigAddBookmark(source(), documentTitle());
}

void UIHelpBrowserTab::sltAnchorClicked(const QUrl &link)
{
    Q_UNUSED(link);
}

void UIHelpBrowserTab::sltCloseFindInPageWidget()
{
    if (m_pFindInPageAction)
        m_pFindInPageAction->setChecked(false);
}


/*********************************************************************************************************************************
*   UIHelpBrowserViewer implementation.                                                                                          *
*********************************************************************************************************************************/

UIHelpBrowserViewer::UIHelpBrowserViewer(const QHelpEngine *pHelpEngine, QWidget *pParent /* = 0 */)
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
            this, &UIHelpBrowserViewer::sltHandleFindWidgetDrag);
    connect(m_pFindInPageWidget, &UIFindInPageWidget::sigSearchTextChanged,
            this, &UIHelpBrowserViewer::sltHandleFindInPageSearchTextChange);

    connect(m_pFindInPageWidget, &UIFindInPageWidget::sigSelectPreviousMatch,
            this, &UIHelpBrowserViewer::sltSelectPreviousMatch);
    connect(m_pFindInPageWidget, &UIFindInPageWidget::sigSelectNextMatch,
            this, &UIHelpBrowserViewer::sltSelectNextMatch);
    connect(m_pFindInPageWidget, &UIFindInPageWidget::sigClose,
            this, &UIHelpBrowserViewer::sigCloseFindInPageWidget);

    m_pFindInPageWidget->setVisible(false);
    retranslateUi();
}

QVariant UIHelpBrowserViewer::loadResource(int type, const QUrl &name)
{
    if (name.scheme() == "qthelp" && m_pHelpEngine)
        return QVariant(m_pHelpEngine->fileData(name));
    else
        return QTextBrowser::loadResource(type, name);
}

void UIHelpBrowserViewer::emitHistoryChangedSignal()
{
    emit historyChanged();
    emit backwardAvailable(true);
}

void UIHelpBrowserViewer::setSource(const QUrl &url)
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

void UIHelpBrowserViewer::toggleFindInPageWidget(bool fVisible)
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

int UIHelpBrowserViewer::initialFontPointSize() const
{
    return m_iInitialFontPointSize;
}

void UIHelpBrowserViewer::setFont(const QFont &font)
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

void UIHelpBrowserViewer::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu pMenu;

    UIContextMenuNavigationAction *pNavigationActions = new UIContextMenuNavigationAction;
    pNavigationActions->setBackwardAvailable(isBackwardAvailable());
    pNavigationActions->setForwardAvailable(isForwardAvailable());

    connect(pNavigationActions, &UIContextMenuNavigationAction::sigGoBackward,
            this, &UIHelpBrowserViewer::sigGoBackward);
    connect(pNavigationActions, &UIContextMenuNavigationAction::sigGoForward,
            this, &UIHelpBrowserViewer::sigGoForward);
    connect(pNavigationActions, &UIContextMenuNavigationAction::sigGoHome,
            this, &UIHelpBrowserViewer::sigGoHome);
    connect(pNavigationActions, &UIContextMenuNavigationAction::sigAddBookmark,
            this, &UIHelpBrowserViewer::sigAddBookmark);

    QAction *pOpenLinkAction = new QAction(UIHelpBrowserWidget::tr("Open Link"));
    connect(pOpenLinkAction, &QAction::triggered,
            this, &UIHelpBrowserViewer::sltHandleOpenLink);

    QAction *pOpenInNewTabAction = new QAction(UIHelpBrowserWidget::tr("Open Link in New Tab"));
    connect(pOpenInNewTabAction, &QAction::triggered,
            this, &UIHelpBrowserViewer::sltHandleOpenLinkInNewTab);

    QAction *pCopyLink = new QAction(UIHelpBrowserWidget::tr("Copy Link"));
    connect(pCopyLink, &QAction::triggered,
            this, &UIHelpBrowserViewer::sltHandleCopyLink);

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

void UIHelpBrowserViewer::resizeEvent(QResizeEvent *pEvent)
{
    /* Make sure the widget stays inside the parent during parent resize: */
    if (m_pFindInPageWidget)
    {
        if (!isRectInside(m_pFindInPageWidget->geometry(), m_iMarginForFindWidget))
            moveFindWidgetIn(m_iMarginForFindWidget);
    }
    QIWithRetranslateUI<QTextBrowser>::resizeEvent(pEvent);
}

void UIHelpBrowserViewer::wheelEvent(QWheelEvent *pEvent)
{
    int iPreviousSize = font().pointSize();
    QTextBrowser::wheelEvent(pEvent);
    /* Don't allow font size to get too large or small: */
    if (font().pointSize() >= fontScaleMinMax.second * m_iInitialFontPointSize ||
        font().pointSize() <= fontScaleMinMax.first * m_iInitialFontPointSize)
    {
        QFont mFont = font();
        mFont.setPointSize(iPreviousSize);
        setFont(mFont);
    }
    else
        emit sigFontPointSizeChanged(font().pointSize());
}

void UIHelpBrowserViewer::retranslateUi()
{
}

void UIHelpBrowserViewer::moveFindWidgetIn(int iMargin)
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

bool UIHelpBrowserViewer::isRectInside(const QRect &rect, int iMargin) const
{
    if (rect.left() < iMargin || rect.top() < iMargin)
        return false;
    if (rect.right() > width() - iMargin || rect.bottom() > height() - iMargin)
        return false;
    return true;
}

void UIHelpBrowserViewer::findAllMatches(const QString &searchString)
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

void UIHelpBrowserViewer::highlightFinds(int iSearchTermLength)
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

void UIHelpBrowserViewer::selectMatch(int iMatchIndex, int iSearchStringLength)
{
    QTextCursor cursor = textCursor();
    /* Move the cursor to the beginning of the matched string: */
    cursor.setPosition(m_matchedCursorPosition.at(iMatchIndex), QTextCursor::MoveAnchor);
    /* Move the cursor to the end of the matched string while keeping the anchor at the begining thus selecting the text: */
    cursor.setPosition(m_matchedCursorPosition.at(iMatchIndex) + iSearchStringLength, QTextCursor::KeepAnchor);
    ensureCursorVisible();
    setTextCursor(cursor);
}

void UIHelpBrowserViewer::sltHandleOpenLinkInNewTab()
{
    QAction *pSender = qobject_cast<QAction*>(sender());
    if (!pSender)
        return;
    QUrl url = pSender->data().toUrl();
    if (url.isValid())
        emit sigOpenLinkInNewTab(url);
}

void UIHelpBrowserViewer::sltHandleOpenLink()
{
    QAction *pSender = qobject_cast<QAction*>(sender());
    if (!pSender)
        return;
    QUrl url = pSender->data().toUrl();
    if (url.isValid())
        QTextBrowser::setSource(url);
}

void UIHelpBrowserViewer::sltHandleCopyLink()
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

void UIHelpBrowserViewer::sltHandleFindWidgetDrag(const QPoint &delta)
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

void UIHelpBrowserViewer::sltHandleFindInPageSearchTextChange(const QString &strSearchText)
{
    m_iSearchTermLength = strSearchText.length();
    findAllMatches(strSearchText);
    highlightFinds(m_iSearchTermLength);
    //scrollToMatch(int iMatchIndex);
    selectMatch(0, m_iSearchTermLength);
    if (m_pFindInPageWidget)
        m_pFindInPageWidget->setMatchCountAndCurrentIndex(m_matchedCursorPosition.size(), 0);
}

void UIHelpBrowserViewer::sltSelectPreviousMatch()
{
    m_iSelectedMatchIndex = m_iSelectedMatchIndex <= 0 ? m_matchedCursorPosition.size() - 1 : (m_iSelectedMatchIndex - 1);
    selectMatch(m_iSelectedMatchIndex, m_iSearchTermLength);
    if (m_pFindInPageWidget)
        m_pFindInPageWidget->setMatchCountAndCurrentIndex(m_matchedCursorPosition.size(), m_iSelectedMatchIndex);
}

void UIHelpBrowserViewer::sltSelectNextMatch()
{
    m_iSelectedMatchIndex = m_iSelectedMatchIndex >= m_matchedCursorPosition.size() - 1 ? 0 : (m_iSelectedMatchIndex + 1);
    selectMatch(m_iSelectedMatchIndex, m_iSearchTermLength);
    if (m_pFindInPageWidget)
        m_pFindInPageWidget->setMatchCountAndCurrentIndex(m_matchedCursorPosition.size(), m_iSelectedMatchIndex);
}


/*********************************************************************************************************************************
*   UIHelpBrowserTabManager definition.                                                                                          *
*********************************************************************************************************************************/

UIHelpBrowserTabManager::UIHelpBrowserTabManager(const QHelpEngine  *pHelpEngine, const QUrl &homeUrl,
                                                 const QStringList &urlList, QWidget *pParent /* = 0 */)
    : QITabWidget(pParent)
    , m_pHelpEngine(pHelpEngine)
    , m_pFontScaleWidget(0)
    , m_homeUrl(homeUrl)
    , m_savedUrlList(urlList)
    , m_fSwitchToNewTab(true)
    , m_fToolBarVisible(true)
{
    prepare();
}

void UIHelpBrowserTabManager::addNewTab(const QUrl &initialUrl)
{
   UIHelpBrowserTab *pTabWidget = new  UIHelpBrowserTab(m_pHelpEngine, m_homeUrl, initialUrl);
   AssertReturnVoid(pTabWidget);
   pTabWidget->setToolBarVisible(m_fToolBarVisible);
   int index = addTab(pTabWidget, pTabWidget->documentTitle());
   connect(pTabWidget, &UIHelpBrowserTab::sigSourceChanged,
           this, &UIHelpBrowserTabManager::sigSourceChanged);
   connect(pTabWidget, &UIHelpBrowserTab::sigTitleUpdate,
           this, &UIHelpBrowserTabManager::sltHandletabTitleChange);
   connect(pTabWidget, &UIHelpBrowserTab::sigOpenLinkInNewTab,
           this, &UIHelpBrowserTabManager::sltHandleOpenLinkInNewTab);
   connect(pTabWidget, &UIHelpBrowserTab::sigAddBookmark,
           this, &UIHelpBrowserTabManager::sigAddBookmark);
   connect(pTabWidget, &UIHelpBrowserTab::sigFontPointSizeChanged,
           this, &UIHelpBrowserTabManager::sltHandleFontSizeChange);
   connect(pTabWidget, &UIHelpBrowserTab::sigLinkHighlighted,
           this, &UIHelpBrowserTabManager::sigLinkHighlighted);

   if (m_fSwitchToNewTab)
       setCurrentIndex(index);

   if (!m_pFontScaleWidget)
   {
       m_pFontScaleWidget = new UIFontScaleWidget(initialFontPointSize(), this);
       connect(m_pFontScaleWidget, &UIFontScaleWidget::sigFontPointSizeChanged,
               this, &UIHelpBrowserTabManager::sltHandleFontSizeChange);
   }
}

void UIHelpBrowserTabManager::updateTabUrlTitleList()
{
    QList<QPair<QString, int> > newList;

    QStringList titles = tabTitleList();

    if (titles == m_tabTitleList)
        return;

    m_tabTitleList = titles;
    emit sigTabsListChanged(m_tabTitleList);
}

void UIHelpBrowserTabManager::closeAllTabsBut(int iTabIndex)
{
    QString strTitle = tabText(iTabIndex);
    QList<QWidget*> widgetList;
    for (int i = 0; i < count(); ++i)
        widgetList.append(widget(i));
    clear();
    for (int i = 0; i < widgetList.size(); ++i)
    {
        if (i != iTabIndex)
            delete widgetList[i];
    }
    addTab(widgetList[iTabIndex], strTitle);
    updateTabUrlTitleList();
}

void UIHelpBrowserTabManager::initializeTabs()
{
    clearAndDeleteTabs();
    /* Start with a single tab showing the home URL: */
    if (m_savedUrlList.isEmpty())
        addNewTab(QUrl());
    /* Start with saved tab(s): */
    else
        for (int i = 0; i < m_savedUrlList.size(); ++i)
            addNewTab(m_savedUrlList[i]);
    updateTabUrlTitleList();
}

QUrl UIHelpBrowserTabManager::currentSource() const
{
    UIHelpBrowserTab *pTab = qobject_cast<UIHelpBrowserTab*>(currentWidget());
    if (!pTab)
        return QUrl();
    return pTab->source();
}

void UIHelpBrowserTabManager::setSource(const QUrl &url, bool fNewTab /* = false */)
{
    if (!fNewTab)
    {
        UIHelpBrowserTab *pTab = qobject_cast<UIHelpBrowserTab*>(currentWidget());
        if (!pTab)
            return;
        pTab->setSource(url);
    }
    else
        addNewTab(url);

    updateTabUrlTitleList();
}

QStringList UIHelpBrowserTabManager::tabUrlList() const
{
    QStringList list;
    for (int i = 0; i < count(); ++i)
    {
        UIHelpBrowserTab *pTab = qobject_cast<UIHelpBrowserTab*>(widget(i));
        if (!pTab || !pTab->source().isValid())
            continue;
        list << pTab->source().toString();
    }
    return list;
}

QStringList UIHelpBrowserTabManager::tabTitleList() const
{
    QStringList list;
    for (int i = 0; i < count(); ++i)
    {
        UIHelpBrowserTab *pTab = qobject_cast<UIHelpBrowserTab*>(widget(i));
        if (!pTab || !pTab->source().isValid())
            continue;
        list << pTab->documentTitle();
    }
    return list;
}


void UIHelpBrowserTabManager::setToolBarVisible(bool fVisible)
{
    /* Make sure existing tabs are configured: */
    for (int i = 0; i < count(); ++i)
    {
        UIHelpBrowserTab *pTab = qobject_cast<UIHelpBrowserTab*>(widget(i));
        if (!pTab)
            continue;
        pTab->setToolBarVisible(fVisible);
    }
    /* This is for the tabs that will be created later: */
    m_fToolBarVisible = fVisible;
}

void UIHelpBrowserTabManager::printCurrent(QPrinter &printer)
{
    UIHelpBrowserTab *pTab = qobject_cast<UIHelpBrowserTab*>(currentWidget());
    if (!pTab)
        return;
    return pTab->print(printer);
}

void UIHelpBrowserTabManager::paintEvent(QPaintEvent *pEvent)
{
    if (m_pFontScaleWidget)
    {
        int iMargin = 20;
        m_pFontScaleWidget->move(width() - m_pFontScaleWidget->width() - iMargin,
                                 height() - m_pFontScaleWidget->height() - iMargin);
    }
    QITabWidget::paintEvent(pEvent);
}

int UIHelpBrowserTabManager::initialFontPointSize() const
{
    UIHelpBrowserTab *pTab = qobject_cast<UIHelpBrowserTab*>(currentWidget());
    if (!pTab)
        return 0;
    return pTab->initialFontPointSize();
}

void UIHelpBrowserTabManager::setFontPointSize(int iPointSize)
{
    for (int i = 0; i < count(); ++i)
    {
        UIHelpBrowserTab *pTab = qobject_cast<UIHelpBrowserTab*>(widget(i));
        if (!pTab)
            continue;
        pTab->setFontPointSize(iPointSize);
    }
    if (m_pFontScaleWidget)
        m_pFontScaleWidget->setFontPointSize(iPointSize);
}

int UIHelpBrowserTabManager::fontPointSize() const
{
    UIHelpBrowserTab *pTab = qobject_cast<UIHelpBrowserTab*>(currentWidget());
    if (!pTab)
        return 0;
    return pTab->fontPointSize();
}

void UIHelpBrowserTabManager::setFontScaleWidgetVisible(bool fToggled)
{
    if (m_pFontScaleWidget)
        m_pFontScaleWidget->setVisible(fToggled);
}

void UIHelpBrowserTabManager::switchToTab(int iIndex)
{
    if (iIndex == currentIndex())
        return;
    setCurrentIndex(iIndex);
}

void UIHelpBrowserTabManager::sltHandletabTitleChange(const QString &strTitle)
{
    for (int i = 0; i < count(); ++i)
    {
        if (sender() == widget(i))
        {
            setTabText(i, strTitle);
            setTabToolTip(i, strTitle);
            continue;
        }
    }
    updateTabUrlTitleList();
}

void UIHelpBrowserTabManager::sltHandleOpenLinkInNewTab(const QUrl &url)
{
    if (url.isValid())
        addNewTab(url);
    updateTabUrlTitleList();
}

void UIHelpBrowserTabManager::sltHandleTabClose(int iTabIndex)
{
    if (count() <= 1)
        return;
    QWidget *pWidget = widget(iTabIndex);
    if (!pWidget)
        return;
    removeTab(iTabIndex);
    delete pWidget;
    updateTabUrlTitleList();
}

void UIHelpBrowserTabManager::sltHandleContextMenuTabClose()
{
    QAction *pAction = qobject_cast<QAction*>(sender());
    if (!pAction)
        return;
    int iTabIndex = pAction->data().toInt();
    if (iTabIndex < 0 || iTabIndex >= count())
        return;
    sltHandleTabClose(iTabIndex);
}

void UIHelpBrowserTabManager::sltHandleCloseOtherTabsContextMenuAction()
{
    /* Find the index of the sender tab. we will close all tabs but sender tab: */
    QAction *pAction = qobject_cast<QAction*>(sender());
    if (!pAction)
        return;
    int iTabIndex = pAction->data().toInt();
    if (iTabIndex < 0 || iTabIndex >= count())
        return;
    closeAllTabsBut(iTabIndex);
}

void UIHelpBrowserTabManager::sltHandleCloseCurrentTab()
{
    sltHandleTabClose(currentIndex());
}

void UIHelpBrowserTabManager::sltHandleCloseOtherTabs()
{
    closeAllTabsBut(currentIndex());
}

void UIHelpBrowserTabManager::sltHandleCurrentChanged(int iTabIndex)
{
    Q_UNUSED(iTabIndex);
    emit sigSourceChanged(currentSource());
}

void UIHelpBrowserTabManager::sltHandleFontSizeChange(int iFontPointSize)
{
    setFontPointSize(iFontPointSize);
}

void UIHelpBrowserTabManager::sltShowTabBarContextMenu(const QPoint &pos)
{
    if (!tabBar())
        return;
    QMenu menu;
    QAction *pCloseAll = menu.addAction(UIHelpBrowserWidget::tr("Close Other Tabs"));
    connect(pCloseAll, &QAction::triggered, this, &UIHelpBrowserTabManager::sltHandleCloseOtherTabsContextMenuAction);
    pCloseAll->setData(tabBar()->tabAt(pos));

    QAction *pClose = menu.addAction(UIHelpBrowserWidget::tr("Close Tab"));
    connect(pClose, &QAction::triggered, this, &UIHelpBrowserTabManager::sltHandleContextMenuTabClose);
    pClose->setData(tabBar()->tabAt(pos));

    menu.exec(tabBar()->mapToGlobal(pos));
}

void UIHelpBrowserTabManager::prepare()
{
    setTabsClosable(true);
    setTabBarAutoHide(true);
    connect(this, &UIHelpBrowserTabManager::tabCloseRequested, this, &UIHelpBrowserTabManager::sltHandleTabClose);
    connect(this, &UIHelpBrowserTabManager::currentChanged, this, &UIHelpBrowserTabManager::sltHandleCurrentChanged);
    if (tabBar())
    {
        tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(tabBar(), &QTabBar::customContextMenuRequested, this, &UIHelpBrowserTabManager::sltShowTabBarContextMenu);
    }
}

void UIHelpBrowserTabManager::clearAndDeleteTabs()
{
    QList<QWidget*> tabList;
    for (int i = 0; i < count(); ++i)
        tabList << widget(i);
    /* QTabWidget::clear() does not delete tab widgets: */
    clear();
    foreach (QWidget *pWidget, tabList)
        delete pWidget;
}


/*********************************************************************************************************************************
*   UIHelpBrowserWidget implementation.                                                                                          *
*********************************************************************************************************************************/

UIHelpBrowserWidget::UIHelpBrowserWidget(EmbedTo enmEmbedding, const QString &strHelpFilePath, QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_enmEmbedding(enmEmbedding)
    , m_fIsPolished(false)
    , m_pMainLayout(0)
    , m_pTopLayout(0)
    , m_pTabWidget(0)
    , m_pToolBar(0)
    , m_strHelpFilePath(strHelpFilePath)
    , m_pHelpEngine(0)
    , m_pSplitter(0)
    , m_pFileMenu(0)
    , m_pViewMenu(0)
    , m_pTabsMenu(0)
    , m_pContentWidget(0)
    , m_pIndexWidget(0)
    , m_pContentModel(0)
    , m_pSearchEngine(0)
    , m_pSearchQueryWidget(0)
    , m_pSearchResultWidget(0)
    , m_pTabManager(0)
    , m_pBookmarksWidget(0)
    , m_pSearchContainerWidget(0)
    , m_pPrintAction(0)
    , m_pShowHideSideBarAction(0)
    , m_pShowHideToolBarAction(0)
    , m_pShowHideFontScaleWidgetAction(0)
    , m_pShowHideStatusBarAction(0)
    , m_pFontSizeLargerAction(0)
    , m_pFontSizeSmallerAction(0)
    , m_pFontSizeResetAction(0)
    , m_fModelContentCreated(false)
    , m_fIndexingFinished(false)
{
    qRegisterMetaType<HelpBrowserTabs>("HelpBrowserTabs");
    prepare();
    loadOptions();
}

UIHelpBrowserWidget::~UIHelpBrowserWidget()
{
    cleanup();
}

QList<QMenu*> UIHelpBrowserWidget::menus() const
{
    QList<QMenu*> menuList;
    menuList
        << m_pFileMenu
        << m_pViewMenu
        << m_pTabsMenu;
    return menuList;
}

void UIHelpBrowserWidget::showHelpForKeyword(const QString &strKeyword)
{
    if (m_fIndexingFinished)
        findAndShowUrlForKeyword(strKeyword);
    else
        m_keywordList.append(strKeyword);
}

bool UIHelpBrowserWidget::shouldBeMaximized() const
{
    return gEDataManager->logWindowShouldBeMaximized();
}

void UIHelpBrowserWidget::prepare()
{
    m_pMainLayout = new QVBoxLayout(this);
    m_pMainLayout->setContentsMargins(0.2 * qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin),
                                      qApp->style()->pixelMetric(QStyle::PM_LayoutTopMargin),
                                      0.2 * qApp->style()->pixelMetric(QStyle::PM_LayoutRightMargin),
                                      0.2 * qApp->style()->pixelMetric(QStyle::PM_LayoutBottomMargin));

    AssertReturnVoid(m_pMainLayout);

    prepareActions();
    prepareMenu();
    prepareWidgets();
    prepareSearchWidgets();
    loadBookmarks();
    retranslateUi();
}

void UIHelpBrowserWidget::prepareActions()
{
    m_pShowHideSideBarAction = new QAction(this);
    m_pShowHideSideBarAction->setCheckable(true);
    m_pShowHideSideBarAction->setChecked(true);
    connect(m_pShowHideSideBarAction, &QAction::toggled,
            this, &UIHelpBrowserWidget::sltHandleWidgetVisibilityToggle);

    m_pShowHideToolBarAction = new QAction(this);
    m_pShowHideToolBarAction->setCheckable(true);
    m_pShowHideToolBarAction->setChecked(true);
    connect(m_pShowHideToolBarAction, &QAction::toggled,
            this, &UIHelpBrowserWidget::sltHandleWidgetVisibilityToggle);

    m_pShowHideFontScaleWidgetAction = new QAction(this);
    m_pShowHideFontScaleWidgetAction->setCheckable(true);
    m_pShowHideFontScaleWidgetAction->setChecked(true);
    connect(m_pShowHideFontScaleWidgetAction, &QAction::toggled,
            this, &UIHelpBrowserWidget::sltHandleWidgetVisibilityToggle);

    m_pShowHideStatusBarAction = new QAction(this);
    m_pShowHideStatusBarAction->setCheckable(true);
    m_pShowHideStatusBarAction->setChecked(true);
    connect(m_pShowHideStatusBarAction, &QAction::toggled,
            this, &UIHelpBrowserWidget::sltHandleWidgetVisibilityToggle);


    m_pPrintAction = new QAction(this);
    connect(m_pPrintAction, &QAction::triggered,
            this, &UIHelpBrowserWidget::sltShowPrintDialog);

    m_pCloseDialogAction = new QAction(this);
    connect(m_pCloseDialogAction, &QAction::triggered,
            this, &UIHelpBrowserWidget::sigCloseDialog);

    /* For size control actions: */
    m_pFontSizeLargerAction = new QAction(this);
    m_pFontSizeLargerAction->setIcon(UIIconPool::iconSet(":/help_browser_plus_32px.png"));

    m_pFontSizeSmallerAction = new QAction(this);
    m_pFontSizeSmallerAction->setIcon(UIIconPool::iconSet(":/help_browser_minus_32px.png"));

    m_pFontSizeResetAction = new QAction(this);
    m_pFontSizeResetAction->setIcon(UIIconPool::iconSet(":/help_browser_reset_32px.png"));

    connect(m_pFontSizeLargerAction, &QAction::triggered,
            this, &UIHelpBrowserWidget::sltHandleFontSizeActions);
    connect(m_pFontSizeSmallerAction, &QAction::triggered,
            this, &UIHelpBrowserWidget::sltHandleFontSizeActions);
    connect(m_pFontSizeResetAction, &QAction::triggered,
            this, &UIHelpBrowserWidget::sltHandleFontSizeActions);
}

void UIHelpBrowserWidget::prepareWidgets()
{
    m_pSplitter = new QSplitter;
    AssertReturnVoid(m_pSplitter);

    m_pMainLayout->addWidget(m_pSplitter);
    m_pHelpEngine = new QHelpEngine(m_strHelpFilePath, this);
    m_pBookmarksWidget = new UIBookmarksListContainer(this);
    m_pTabWidget = new QITabWidget;
    m_pTabManager = new UIHelpBrowserTabManager(m_pHelpEngine, findIndexHtml(), loadSavedUrlList());

    AssertReturnVoid(m_pTabWidget &&
                     m_pHelpEngine &&
                     m_pBookmarksWidget &&
                     m_pTabManager);

    m_pContentWidget = m_pHelpEngine->contentWidget();
    m_pIndexWidget = m_pHelpEngine->indexWidget();
    m_pContentModel = m_pHelpEngine->contentModel();

    AssertReturnVoid(m_pContentWidget && m_pIndexWidget && m_pContentModel);
    m_pSplitter->addWidget(m_pTabWidget);
    m_pContentWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    m_pTabWidget->insertTab(HelpBrowserTabs_TOC, m_pContentWidget, QString());
    m_pTabWidget->insertTab(HelpBrowserTabs_Bookmarks, m_pBookmarksWidget, QString());
    m_pTabWidget->insertTab(HelpBrowserTabs_Index, m_pIndexWidget, QString());

    m_pSplitter->addWidget(m_pTabManager);
    m_pSplitter->setStretchFactor(0,0);
    m_pSplitter->setStretchFactor(1,1);

    m_pSplitter->setChildrenCollapsible(false);

    connect(m_pTabManager, &UIHelpBrowserTabManager::sigSourceChanged,
            this, &UIHelpBrowserWidget::sltHandleHelpBrowserViewerSourceChange);
    connect(m_pTabManager, &UIHelpBrowserTabManager::sigAddBookmark,
            this, &UIHelpBrowserWidget::sltAddNewBookmark);
    connect(m_pTabManager, &UIHelpBrowserTabManager::sigTabsListChanged,
            this, &UIHelpBrowserWidget::sltHandleTabListChanged);
    connect(m_pTabManager, &UIHelpBrowserTabManager::currentChanged,
            this, &UIHelpBrowserWidget::sltHandleCurrentTabChanged);
    connect(m_pTabManager, &UIHelpBrowserTabManager::sigLinkHighlighted,
            this, &UIHelpBrowserWidget::sigLinkHighlighted);

    connect(m_pHelpEngine, &QHelpEngine::setupFinished,
            this, &UIHelpBrowserWidget::sltHandleHelpEngineSetupFinished);
    connect(m_pContentWidget, &QHelpContentWidget::clicked,
            this, &UIHelpBrowserWidget::sltHandleContentWidgetItemClicked);
    connect(m_pContentModel, &QHelpContentModel::contentsCreated,
            this, &UIHelpBrowserWidget::sltHandleContentsCreated);
    connect(m_pContentWidget, &QHelpContentWidget::customContextMenuRequested,
            this, &UIHelpBrowserWidget::sltShowLinksContextMenu);
    connect(m_pBookmarksWidget, &UIBookmarksListContainer::sigBookmarkDoubleClick,
            this, &UIHelpBrowserWidget::sltOpenLinkWithUrl);
    connect(m_pBookmarksWidget, &UIBookmarksListContainer::sigListWidgetContextMenuRequest,
            this, &UIHelpBrowserWidget::sltShowLinksContextMenu);

    if (QFile(m_strHelpFilePath).exists() && m_pHelpEngine)
        m_pHelpEngine->setupData();
}

void UIHelpBrowserWidget::prepareSearchWidgets()
{
    AssertReturnVoid(m_pTabWidget && m_pHelpEngine);

    m_pSearchContainerWidget = new QWidget;
    m_pTabWidget->insertTab(HelpBrowserTabs_Search, m_pSearchContainerWidget, QString());
    m_pTabWidget->setTabPosition(QTabWidget::South);


    m_pSearchEngine = m_pHelpEngine->searchEngine();
    AssertReturnVoid(m_pSearchEngine);

    m_pSearchQueryWidget = m_pSearchEngine->queryWidget();
    m_pSearchResultWidget = m_pSearchEngine->resultWidget();
    AssertReturnVoid(m_pSearchQueryWidget && m_pSearchResultWidget);
    m_pSearchResultWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_pSearchQueryWidget->setCompactMode(false);

    QVBoxLayout *pSearchLayout = new QVBoxLayout(m_pSearchContainerWidget);
    pSearchLayout->addWidget(m_pSearchQueryWidget);
    pSearchLayout->addWidget(m_pSearchResultWidget);
    m_pSearchQueryWidget->expandExtendedSearch();

    connect(m_pSearchQueryWidget, &QHelpSearchQueryWidget::search,
            this, &UIHelpBrowserWidget::sltHandleSearchStart);
    connect(m_pSearchResultWidget, &QHelpSearchResultWidget::requestShowLink,
            this, &UIHelpBrowserWidget::sltOpenLinkWithUrl);
    connect(m_pSearchResultWidget, &QHelpContentWidget::customContextMenuRequested,
            this, &UIHelpBrowserWidget::sltShowLinksContextMenu);
    connect(m_pSearchEngine, &QHelpSearchEngine::indexingStarted,
            this, &UIHelpBrowserWidget::sltHandleIndexingStarted);
    connect(m_pSearchEngine, &QHelpSearchEngine::indexingFinished,
            this, &UIHelpBrowserWidget::sltHandleIndexingFinished);
    connect(m_pSearchEngine, &QHelpSearchEngine::searchingStarted,
            this, &UIHelpBrowserWidget::sltHandleSearchingStarted);

    m_pSearchEngine->reindexDocumentation();
}

void UIHelpBrowserWidget::prepareToolBar()
{
    m_pTopLayout = new QHBoxLayout;
    m_pToolBar = new QIToolBar(parentWidget());
    if (m_pToolBar)
    {
        m_pToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
        const int iIconMetric = (int)(QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize));
        m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));

#ifdef VBOX_WS_MAC
        /* Check whether we are embedded into a stack: */
        if (m_enmEmbedding == EmbedTo_Stack)
        {
            /* Add into layout: */
            m_pTopLayout->addWidget(m_pToolBar);
            m_pMainLayout->addLayout(m_pTopLayout);
        }
#else
        /* Add into layout: */
        m_pTopLayout->addWidget(m_pToolBar);
        m_pMainLayout->addLayout(m_pTopLayout);
#endif
    }
}

void UIHelpBrowserWidget::prepareMenu()
{
    m_pFileMenu = new QMenu(tr("File"), this);
    m_pViewMenu = new QMenu(tr("View"), this);
    m_pTabsMenu = new QMenu(tr("Tabs"), this);

    AssertReturnVoid(m_pFileMenu && m_pViewMenu && m_pTabsMenu);

    if (m_pPrintAction)
        m_pFileMenu->addAction(m_pPrintAction);
    if (m_pCloseDialogAction)
        m_pFileMenu->addAction(m_pCloseDialogAction);

    if (m_pFontSizeLargerAction)
        m_pViewMenu->addAction(m_pFontSizeLargerAction);
    if (m_pFontSizeSmallerAction)
        m_pViewMenu->addAction(m_pFontSizeSmallerAction);
    if (m_pFontSizeResetAction)
        m_pViewMenu->addAction(m_pFontSizeResetAction);
    m_pViewMenu->addSeparator();
    if (m_pShowHideSideBarAction)
        m_pViewMenu->addAction(m_pShowHideSideBarAction);
    if (m_pShowHideToolBarAction)
        m_pViewMenu->addAction(m_pShowHideToolBarAction);
    if (m_pShowHideFontScaleWidgetAction)
        m_pViewMenu->addAction(m_pShowHideFontScaleWidgetAction);
    if (m_pShowHideStatusBarAction)
        m_pViewMenu->addAction(m_pShowHideStatusBarAction);
}

void UIHelpBrowserWidget::loadOptions()
{
}

QStringList UIHelpBrowserWidget::loadSavedUrlList()
{
    return gEDataManager->helpBrowserLastUrlList();
}

void UIHelpBrowserWidget::loadBookmarks()
{
    if (!m_pBookmarksWidget)
        return;

    QStringList bookmarks = gEDataManager->helpBrowserBookmarks();
    /* bookmarks list is supposed to have url title pair: */
    for (int i = 0; i < bookmarks.size(); ++i)
    {
        const QString &url = bookmarks[i];
        if (i+1 >= bookmarks.size())
            break;
        ++i;
        const QString &strTitle = bookmarks[i];
        m_pBookmarksWidget->addBookmark(url, strTitle);
    }
}

void UIHelpBrowserWidget::saveBookmarks()
{
    if (!m_pBookmarksWidget)
        return;
    gEDataManager->setHelpBrowserBookmarks(m_pBookmarksWidget->bookmarks());
}

void UIHelpBrowserWidget::saveOptions()
{
    if (m_pTabManager)
        gEDataManager->setHelpBrowserLastUrlList(m_pTabManager->tabUrlList());
}

QUrl UIHelpBrowserWidget::findIndexHtml() const
{
    QList<QUrl> files = m_pHelpEngine->files(m_pHelpEngine->namespaceName(m_strHelpFilePath), QStringList());
    int iIndex = -1;
    for (int i = 0; i < files.size(); ++i)
    {
        if (files[i].toString().contains("index.html", Qt::CaseInsensitive))
        {
            iIndex = i;
            break;
        }
    }
    if (iIndex == -1)
    {
        /* If index html/htm could not be found try to find a html file at least: */
        for (int i = 0; i < files.size(); ++i)
        {
            if (files[i].toString().contains(".html", Qt::CaseInsensitive) ||
                files[i].toString().contains(".htm", Qt::CaseInsensitive))
            {
                iIndex = i;
                break;
            }
        }
    }
    if (iIndex != -1 && files.size() > iIndex)
        return files[iIndex];
    else
        return QUrl();
}

QUrl UIHelpBrowserWidget::contentWidgetUrl(const QModelIndex &itemIndex)
{
    QHelpContentModel *pContentModel =
        qobject_cast<QHelpContentModel*>(m_pContentWidget->model());
    if (!pContentModel)
        return QUrl();
    QHelpContentItem *pItem = pContentModel->contentItemAt(itemIndex);
    if (!pItem)
        return QUrl();
    return pItem->url();
}

void UIHelpBrowserWidget::cleanup()
{
    saveOptions();
    saveBookmarks();
}

void UIHelpBrowserWidget::retranslateUi()
{
    /* Translate toolbar: */
#ifdef VBOX_WS_MAC
    // WORKAROUND:
    // There is a bug in Qt Cocoa which result in showing a "more arrow" when
    // the necessary size of the toolbar is increased. Also for some languages
    // the with doesn't match if the text increase. So manually adjust the size
    // after changing the text. */
    if (m_pToolBar)
        m_pToolBar->updateLayout();
#endif
    if (m_pTabWidget)
    {
        m_pTabWidget->setTabText(HelpBrowserTabs_TOC, tr("Contents"));
        m_pTabWidget->setTabText(HelpBrowserTabs_Index, tr("Index"));
        m_pTabWidget->setTabText(HelpBrowserTabs_Search, tr("Search"));
        m_pTabWidget->setTabText(HelpBrowserTabs_Bookmarks, tr("Bookmarks"));
    }

    if (m_pShowHideSideBarAction)
        m_pShowHideSideBarAction->setText(tr("Show Side Bar"));
    if (m_pShowHideToolBarAction)
        m_pShowHideToolBarAction->setText(tr("Show Tool Bar"));
    if (m_pShowHideFontScaleWidgetAction)
        m_pShowHideFontScaleWidgetAction->setText(tr("Show Font Scale Widget"));
    if (m_pShowHideStatusBarAction)
        m_pShowHideStatusBarAction->setText(tr("Show Status Bar"));


    if (m_pPrintAction)
        m_pPrintAction->setText(tr("Print..."));
    if (m_pCloseDialogAction)
        m_pCloseDialogAction->setText(tr("Close"));

    if (m_pFontSizeLargerAction)
        m_pFontSizeLargerAction->setText(tr("Increase Font Size"));
    if (m_pFontSizeSmallerAction)
        m_pFontSizeSmallerAction->setText(tr("Decrease Font Size"));
    if (m_pFontSizeResetAction)
        m_pFontSizeResetAction->setText(tr("Reset Font Size"));
}


void UIHelpBrowserWidget::showEvent(QShowEvent *pEvent)
{
    QWidget::showEvent(pEvent);
   if (m_fIsPolished)
        return;
    m_fIsPolished = true;
}

void UIHelpBrowserWidget::keyPressEvent(QKeyEvent *pEvent)
{
   QWidget::keyPressEvent(pEvent);
}

void UIHelpBrowserWidget::findAndShowUrlForKeyword(const QString &strKeyword)
{
    QMap<QString, QUrl> map = m_pHelpEngine->linksForIdentifier(strKeyword);
    if (!map.isEmpty())
    {
        /* We have to a have a single url per keyword in this case: */
        QUrl keywordUrl = map.first();
        m_pTabManager->setSource(keywordUrl, true /* new tab */);
    }
}

void UIHelpBrowserWidget::sltHandleWidgetVisibilityToggle(bool fToggled)
{
    if (sender() == m_pShowHideSideBarAction)
    {
        if (m_pTabWidget)
            m_pTabWidget->setVisible(fToggled);
    }
    else if (sender() == m_pShowHideToolBarAction)
    {
        if (m_pTabManager)
            m_pTabManager->setToolBarVisible(fToggled);
    }
    else if (sender() == m_pShowHideFontScaleWidgetAction)
    {
        if (m_pTabManager)
            m_pTabManager->setFontScaleWidgetVisible(fToggled);
    }
    else if (sender() == m_pShowHideStatusBarAction)
        emit sigStatusBarVisible(fToggled);
}

void UIHelpBrowserWidget::sltShowPrintDialog()
{
    if (!m_pTabManager)
        return;
    QPrinter printer;
    QPrintDialog printDialog(&printer, this);
    if (printDialog.exec() == QDialog::Accepted)
        m_pTabManager->printCurrent(printer);
}

void UIHelpBrowserWidget::sltHandleHelpEngineSetupFinished()
{
    AssertReturnVoid(m_pTabManager);
    m_fIndexingFinished = true;
    m_pTabManager->initializeTabs();
}

void UIHelpBrowserWidget::sltHandleContentWidgetItemClicked(const QModelIndex & index)
{
    AssertReturnVoid(m_pTabManager && m_pHelpEngine && m_pContentWidget);
    QUrl url = contentWidgetUrl(index);
    if (!url.isValid())
        return;
    m_pTabManager->setSource(url);

    m_pContentWidget->scrollTo(index, QAbstractItemView::EnsureVisible);
    m_pContentWidget->expand(index);
}

void UIHelpBrowserWidget::sltHandleHelpBrowserViewerSourceChange(const QUrl &source)
{
    if (m_fModelContentCreated && m_pContentWidget && source.isValid() && m_pContentModel)
    {
        QModelIndex index = m_pContentWidget->indexOf(source);
        QItemSelectionModel *pSelectionModel = m_pContentWidget->selectionModel();
        if (pSelectionModel && index.isValid())
        {
            m_pContentWidget->blockSignals(true);
            pSelectionModel->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            m_pContentWidget->scrollTo(index, QAbstractItemView::EnsureVisible);
            m_pContentWidget->expand(index);
            m_pContentWidget->blockSignals(false);
        }
    }
}

void UIHelpBrowserWidget::sltHandleContentsCreated()
{
    m_fModelContentCreated = true;
    if (m_pTabManager)
        sltHandleHelpBrowserViewerSourceChange(m_pTabManager->currentSource());
}

void UIHelpBrowserWidget::sltHandleIndexingStarted()
{
    if (m_pSearchContainerWidget)
        m_pSearchContainerWidget->setEnabled(false);
}

void UIHelpBrowserWidget::sltHandleIndexingFinished()
{
    AssertReturnVoid(m_pTabManager &&
                     m_pHelpEngine &&
                     m_pSearchContainerWidget);

    m_pSearchContainerWidget->setEnabled(true);
    m_fIndexingFinished = true;
    /* Process the keyword queue. */
    foreach (const QString strKeyword, m_keywordList)
        findAndShowUrlForKeyword(strKeyword);
    m_keywordList.clear();

}

void UIHelpBrowserWidget::sltHandleSearchingStarted()
{
}

void UIHelpBrowserWidget::sltHandleSearchStart()
{
    AssertReturnVoid(m_pSearchEngine && m_pSearchQueryWidget);
    m_pSearchEngine->search(m_pSearchQueryWidget->searchInput());
}

void UIHelpBrowserWidget::sltShowLinksContextMenu(const QPoint &pos)
{
    QWidget *pSender = qobject_cast<QWidget*>(sender());
    if (!pSender)
        return;

    QUrl url;
    if (pSender == m_pContentWidget)
        url = contentWidgetUrl(m_pContentWidget->currentIndex());
    else if (pSender == m_pSearchResultWidget)
    {
        QTextBrowser* browser = m_pSearchResultWidget->findChild<QTextBrowser*>();
        if (!browser)
            return;
        QPoint browserPos = browser->mapFromGlobal(m_pSearchResultWidget->mapToGlobal(pos));
        url = browser->anchorAt(browserPos);
    }
    else if (pSender == m_pBookmarksWidget)
    {
        /* Assuming that only the UIBookmarksListWidget under the m_pBookmarksWidget sends the context menu request: */
        UIBookmarksListWidget *pListWidget = m_pBookmarksWidget->findChild<UIBookmarksListWidget*>();
        if (!pListWidget)
            return;
        url = m_pBookmarksWidget->currentBookmarkUrl();
    }
    else
        return;

    if (!url.isValid())
        return;

    QMenu menu;
    QAction *pOpen = menu.addAction(tr("Open in Link"));
    QAction *pOpenInNewTab = menu.addAction(tr("Open in Link New Tab"));
    QAction *pCopyLink = menu.addAction(tr("Copy Link"));

    pOpen->setData(url);
    pOpenInNewTab->setData(url);
    pCopyLink->setData(url);

    connect(pOpenInNewTab, &QAction::triggered, this, &UIHelpBrowserWidget::sltOpenLinkInNewTab);
    connect(pOpen, &QAction::triggered, this, &UIHelpBrowserWidget::sltOpenLink);
    connect(pCopyLink, &QAction::triggered, this, &UIHelpBrowserWidget::sltCopyLink);

    if (pSender == m_pBookmarksWidget)
    {
        menu.addSeparator();
        QAction *pDeleteBookmark = menu.addAction(tr("Delete Bookmark"));
        connect(pDeleteBookmark, &QAction::triggered, m_pBookmarksWidget, &UIBookmarksListContainer::sltDeleteSelectedBookmark);
    }

    menu.exec(pSender->mapToGlobal(pos));
}

void UIHelpBrowserWidget::sltOpenLinkInNewTab()
{
    openLinkSlotHandler(sender(), true);
}

void UIHelpBrowserWidget::sltOpenLink()
{
    openLinkSlotHandler(sender(), false);
}

void UIHelpBrowserWidget::sltCopyLink()
{
    QAction *pAction = qobject_cast<QAction*>(sender());
    if (!pAction)
        return;
    QUrl url = pAction->data().toUrl();
    if (url.isValid())
    {
        QClipboard *pClipboard = QApplication::clipboard();
        if (pClipboard)
            pClipboard->setText(url.toString());
    }
}

void UIHelpBrowserWidget::sltAddNewBookmark(const QUrl &url, const QString &strTitle)
{
    if (m_pBookmarksWidget)
        m_pBookmarksWidget->addBookmark(url, strTitle);
}

void UIHelpBrowserWidget::openLinkSlotHandler(QObject *pSenderObject, bool fOpenInNewTab)
{
    QAction *pAction = qobject_cast<QAction*>(pSenderObject);
    if (!pAction)
        return;
    QUrl url = pAction->data().toUrl();
    if (m_pTabManager && url.isValid())
        m_pTabManager->setSource(url, fOpenInNewTab);
}

void UIHelpBrowserWidget::updateTabsMenu(const QStringList &titles)
{
    if (!m_pTabsMenu)
        return;
    m_pTabsMenu->clear();

    QAction *pCloseTabAction = m_pTabsMenu->addAction(tr("Close Tab"));
    QAction *pCloseOtherTabsAction = m_pTabsMenu->addAction(tr("Close Other Tabs"));

    pCloseTabAction->setEnabled(titles.size() > 1);
    pCloseOtherTabsAction->setEnabled(titles.size() > 1);

    connect(pCloseTabAction, &QAction::triggered, m_pTabManager, &UIHelpBrowserTabManager::sltHandleCloseCurrentTab);
    connect(pCloseOtherTabsAction, &QAction::triggered, m_pTabManager, &UIHelpBrowserTabManager::sltHandleCloseOtherTabs);

    m_pTabsMenu->addSeparator();

    for (int i = 0; i < titles.size(); ++i)
    {
        QAction *pAction = m_pTabsMenu->addAction(titles[i]);
        pAction->setData(i);
        connect(pAction, &QAction::triggered, this, &UIHelpBrowserWidget::sltHandleTabChoose);
    }
    if (m_pTabManager)
        sltHandleCurrentTabChanged(m_pTabManager->currentIndex());
}

void UIHelpBrowserWidget::sltOpenLinkWithUrl(const QUrl &url)
{
    if (m_pTabManager && url.isValid())
        m_pTabManager->setSource(url, false);
}

void UIHelpBrowserWidget::sltHandleFontSizeActions()
{
    if (!sender() || !m_pTabManager)
        return;
    int iFontPointSize = m_pTabManager->fontPointSize();
    if (sender() == m_pFontSizeResetAction)
        iFontPointSize = m_pTabManager->initialFontPointSize();
    else if (sender() == m_pFontSizeLargerAction)
        iFontPointSize += iFontPointSizeChangeStep;
    else if (sender() == m_pFontSizeSmallerAction)
        iFontPointSize -= iFontPointSizeChangeStep;

    if (iFontPointSize >= fontScaleMinMax.second * m_pTabManager->initialFontPointSize() ||
        iFontPointSize <= fontScaleMinMax.first * m_pTabManager->initialFontPointSize())
        return;
    m_pTabManager->setFontPointSize(iFontPointSize);
}

void UIHelpBrowserWidget::sltHandleTabListChanged(const QStringList &titleList)
{
    updateTabsMenu(titleList);
}

void UIHelpBrowserWidget::sltHandleTabChoose()
{
    QAction *pAction = qobject_cast<QAction*>(sender());
    if (!pAction)
        return;
    int iIndex = pAction->data().toInt();
    if (m_pTabManager)
        m_pTabManager->switchToTab(iIndex);
}

void UIHelpBrowserWidget::sltHandleCurrentTabChanged(int iIndex)
{
    Q_UNUSED(iIndex);
    if (!m_pTabsMenu)
        return;

    /** Mark the action with iIndex+3 by assigning an icon to it. it is iIndex+3 and not iIndex since we have
      * two additional (close tab, close other tabs and a separator) action on top of the tab selection actions: */
    QList<QAction*> list = m_pTabsMenu->actions();
    for (int i = 0; i < list.size(); ++i)
        list[i]->setIcon(QIcon());
    if (iIndex+3 >= list.size())
        return;
    list[iIndex+3]->setIcon(UIIconPool::iconSet(":/help_browser_star_16px.png"));
}


#include "UIHelpBrowserWidget.moc"

#endif /*#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP) && (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))*/
