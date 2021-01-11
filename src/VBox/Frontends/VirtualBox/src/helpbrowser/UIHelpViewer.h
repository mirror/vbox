/* $Id$ */
/** @file
 * VBox Qt GUI - UIHelpBrowserWidget class declaration.
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

#ifndef FEQT_INCLUDED_SRC_helpbrowser_UIHelpViewer_h
#define FEQT_INCLUDED_SRC_helpbrowser_UIHelpViewer_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QTextBrowser>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QHelpEngine;
class UIFindInPageWidget;

#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP) && (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))
class UIHelpViewer : public QIWithRetranslateUI<QTextBrowser>
{
    Q_OBJECT;

signals:

    void sigOpenLinkInNewTab(const QUrl &url, bool fBackground);
    void sigCloseFindInPageWidget();
    void sigFontPointSizeChanged(int iFontPointSize);
    void sigGoBackward();
    void sigGoForward();
    void sigGoHome();
    void sigAddBookmark();

public:

    UIHelpViewer(const QHelpEngine *pHelpEngine, QWidget *pParent = 0);
    virtual QVariant loadResource(int type, const QUrl &name) /* override */;
    void emitHistoryChangedSignal();
    void setSource(const QUrl &url) /* override */;
    int initialFontPointSize() const;
    void setFont(const QFont &);
    bool isFindInPageWidgetVisible() const;

public slots:

    void sltToggleFindInPageWidget(bool fVisible);

protected:

    virtual void contextMenuEvent(QContextMenuEvent *event) /* override */;
    virtual void resizeEvent(QResizeEvent *pEvent) /* override */;
    virtual void wheelEvent(QWheelEvent *pEvent) /* override */;
    virtual void mousePressEvent(QMouseEvent *pEvent) /* override */;

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
    int m_iMarginForFindWidget;
    /** Document positions of the cursors within the document for all matches. */
    QVector<int>   m_matchedCursorPosition;
    int m_iSelectedMatchIndex;
    int m_iSearchTermLength;
    int m_iInitialFontPointSize;
};

#endif /* #if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP) && (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)) */
#endif /* !FEQT_INCLUDED_SRC_helpbrowser_UIHelpViewer_h */
