/** @file
 * VBox Qt GUI - UIGraphicsTextPane class declaration.
 */

/*
 * Copyright (C) 2012-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIGraphicsTextPane_h___
#define ___UIGraphicsTextPane_h___

/* GUI includes: */
#include "QIGraphicsWidget.h"

/* Typedefs: */
typedef QPair<QString, QString> UITextTableLine;
typedef QList<UITextTableLine> UITextTable;
Q_DECLARE_METATYPE(UITextTable);

/** QIGraphicsWidget reimplementation to draw QTextLayout content. */
class UIGraphicsTextPane : public QIGraphicsWidget
{
    Q_OBJECT;

public:

    /** Graphics text-pane constructor. */
    UIGraphicsTextPane(QIGraphicsWidget *pParent, QPaintDevice *pPaintDevice);

    /** Returns whether contained text is empty. */
    bool isEmpty() const { return m_text.isEmpty(); }
    /** Returns contained text. */
    const UITextTable& text() const { return m_text; }
    /** Defines contained text. */
    void setText(const UITextTable &text);

private:

    /** Updates minimum text width hint. */
    void updateMinimumTextWidthHint();
    /** Updates minimum text height hint. */
    void updateMinimumTextHeightHint();
    /** Returns the size-hint to constrain the content. */
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;

    /** This event handler is delivered after the widget has been resized. */
    void resizeEvent(QGraphicsSceneResizeEvent *pEvent);

    /** Paints the contents in local coordinates. */
    void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, QWidget *pWidget = 0);

    /** Builds new text-layout. */
    static QTextLayout* buildTextLayout(const QFont &font, QPaintDevice *pPaintDevice,
                                        const QString &strText, int iWidth, int &iHeight);

    /** Margin. */
    const int m_iMargin;
    /** Spacing. */
    const int m_iSpacing;
    /** Minimum text-column width: */
    const int m_iMinimumTextColumnWidth;

    /** Minimum text-width. */
    int m_iMinimumTextWidth;
    /** Minimum text-height. */
    int m_iMinimumTextHeight;

    /** Paint-device to scale to. */
    QPaintDevice *m_pPaintDevice;

    /** Contained text. */
    UITextTable m_text;
};

#endif /* !___UIGraphicsTextPane_h___ */
