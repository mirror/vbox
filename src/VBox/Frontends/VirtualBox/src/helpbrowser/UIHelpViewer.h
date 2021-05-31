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
class QGraphicsBlurEffect;
class QLabel;
class UIFindInPageWidget;

#ifdef VBOX_WITH_QHELP_VIEWER

class UIHelpViewer : public QIWithRetranslateUI<QTextBrowser>
{

    Q_OBJECT;

signals:

    void sigOpenLinkInNewTab(const QUrl &url, bool fBackground);
    void sigFindInPageWidgetToogle(bool fVisible);
    void sigFontPointSizeChanged(int iFontPointSize);
    void sigGoBackward();
    void sigGoForward();
    void sigGoHome();
    void sigAddBookmark();
    void sigZoomPercentageChanged(int iPercentage);
    void sigOverlayModeChanged(bool fEnabled);

public:

    enum ZoomOperation
    {
        ZoomOperation_In = 0,
        ZoomOperation_Out,
        ZoomOperation_Reset,
        ZoomOperation_Max
    };

    UIHelpViewer(const QHelpEngine *pHelpEngine, QWidget *pParent = 0);
    virtual QVariant loadResource(int type, const QUrl &name) /* override */;
    void emitHistoryChangedSignal();
    virtual void setSource(const QUrl &url) /* override */;
    void setFont(const QFont &);
    bool isFindInPageWidgetVisible() const;
    void zoom(ZoomOperation enmZoomOperation);
    int zoomPercentage() const;
    void setZoomPercentage(int iZoomPercentage);
    void setHelpFileList(const QList<QUrl> &helpFileList);
    bool hasSelectedText() const;
    static const QPair<int, int> zoomPercentageMinMax;
    void toggleFindInPageWidget(bool fVisible);

public slots:

    void sltSelectPreviousMatch();
    void sltSelectNextMatch();
    virtual void reload() /* overload */;

protected:

    virtual void contextMenuEvent(QContextMenuEvent *event) /* override */;
    virtual void resizeEvent(QResizeEvent *pEvent) /* override */;
    virtual void wheelEvent(QWheelEvent *pEvent) /* override */;
    virtual void mouseReleaseEvent(QMouseEvent *pEvent) /* override */;
    virtual void mousePressEvent(QMouseEvent *pEvent) /* override */;
    virtual void mouseMoveEvent(QMouseEvent *pEvent) /* override */;
    virtual void mouseDoubleClickEvent(QMouseEvent *pEvent) /* override */;
    virtual void paintEvent(QPaintEvent *pEvent) /* override */;
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;
    virtual void keyPressEvent(QKeyEvent *pEvent) /* override */;

private slots:

    void sltOpenLinkInNewTab();
    void sltOpenLink();
    void sltCopyLink();
    void sltFindWidgetDrag(const QPoint &delta);
    void sltFindInPageSearchTextChange(const QString &strSearchText);
    void sltToggleFindInPageWidget(bool fVisible);
    void sltCloseFindInPageWidget();

private:

    struct DocumentImage
    {
        qreal m_fInitialWidth;
        qreal m_fScaledWidth;
        QTextCursor m_textCursor;
        QPixmap m_pixmap;
        QString m_strName;
    };

    void retranslateUi();
    bool isRectInside(const QRect &rect, int iMargin) const;
    void moveFindWidgetIn(int iMargin);
    void findAllMatches(const QString &searchString);
    void highlightFinds(int iSearchTermLength);
    void selectMatch(int iMatchIndex, int iSearchStringLength);
    void iterateDocumentImages();
    void scaleFont();
    void scaleImages();
    /** If there is image at @p globalPosition then its data is loaded to m_overlayPixmap. */
    void loadImageAtPosition(const QPoint &globalPosition);
    void clearOverlay();

    const QHelpEngine* m_pHelpEngine;
    UIFindInPageWidget *m_pFindInPageWidget;
    /** Initilized as false and set to true once the user drag moves the find widget. */
    bool m_fFindWidgetDragged;
    int m_iMarginForFindWidget;
    /** Document positions of the cursors within the document for all matches. */
    QVector<int>   m_matchedCursorPosition;
    int m_iSelectedMatchIndex;
    int m_iSearchTermLength;
    int m_iInitialFontPointSize;
    /** A container to store the original image sizes/positions in the document. key is image name value is DocumentImage. */
    QHash<QString, DocumentImage> m_imageMap;
    /** As percentage. */
    int m_iZoomPercentage;
    QCursor m_defaultCursor;
    QCursor m_handCursor;
    QList<QUrl> m_helpFileList;
    QPixmap m_overlayPixmap;
    bool m_fOverlayMode;
    bool m_fCursorChanged;
    QLabel *m_pOverlayLabel;
    QGraphicsBlurEffect *m_pOverlayBlurEffect;

};

#endif /* #ifdef VBOX_WITH_QHELP_VIEWER */
#endif /* !FEQT_INCLUDED_SRC_helpbrowser_UIHelpViewer_h */
