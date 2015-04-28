/* $Id$ */
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

/* Qt includes: */
#include <QTextLayout>

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

signals:

    /** Notifies listeners about size-hint changes. */
    void sigGeometryChanged();

    /** Notifies listeners about anchor clicked. */
    void sigAnchorClicked(const QString &strAnchor);

public:

    /** Graphics text-pane constructor. */
    UIGraphicsTextPane(QIGraphicsWidget *pParent, QPaintDevice *pPaintDevice);
    /** Graphics text-pane destructor. */
    ~UIGraphicsTextPane();

    /** Returns whether contained text is empty. */
    bool isEmpty() const { return m_text.isEmpty(); }
    /** Returns contained text. */
    const UITextTable& text() const { return m_text; }
    /** Defines contained text. */
    void setText(const UITextTable &text);

    /** Defines whether passed @a strAnchorRole is @a fRestricted. */
    void setAnchorRoleRestricted(const QString &strAnchorRole, bool fRestricted);

private:

    /** Update text-layout. */
    void updateTextLayout(bool fFull = false);

    /** Notifies listeners about size-hint changes. */
    void updateGeometry();
    /** Returns the size-hint to constrain the content. */
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;

    /** This event handler is delivered after the widget has been resized. */
    void resizeEvent(QGraphicsSceneResizeEvent *pEvent);

    /** This event handler called when mouse hovers widget. */
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *pEvent);
    /** This event handler called when mouse leaves widget. */
    void hoverMoveEvent(QGraphicsSceneHoverEvent *pEvent);
    /** Summarize two hover-event handlers above. */
    void handleHoverEvent(QGraphicsSceneHoverEvent *pEvent);
    /** Update hover stuff. */
    void updateHoverStuff();

    /** This event handler called when mouse press widget. */
    void mousePressEvent(QGraphicsSceneMouseEvent *pEvent);

    /** Paints the contents in local coordinates. */
    void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, QWidget *pWidget = 0);

    /** Builds new text-layout. */
    static QTextLayout* buildTextLayout(const QFont &font, QPaintDevice *pPaintDevice,
                                        const QString &strText, int iWidth, int &iHeight,
                                        const QString &strHoveredAnchor);

    /** Search for hovered anchor in passed text-layout @a list. */
    static QString searchForHoveredAnchor(QPaintDevice *pPaintDevice, const QList<QTextLayout*> &list, const QPoint &mousePosition);

    /** Paint-device to scale to. */
    QPaintDevice *m_pPaintDevice;

    /** Margin. */
    const int m_iMargin;
    /** Spacing. */
    const int m_iSpacing;
    /** Minimum text-column width: */
    const int m_iMinimumTextColumnWidth;

    /** Minimum size-hint invalidation flag. */
    mutable bool m_fMinimumSizeHintInvalidated;
    /** Minimum size-hint cache. */
    mutable QSizeF m_minimumSizeHint;
    /** Minimum text-width. */
    int m_iMinimumTextWidth;
    /** Minimum text-height. */
    int m_iMinimumTextHeight;

    /** Contained text. */
    UITextTable m_text;
    /** Left text-layout list. */
    QList<QTextLayout*> m_leftList;
    /** Right text-layout list. */
    QList<QTextLayout*> m_rightList;

    /** Holds whether anchor can be hovered. */
    bool m_fAnchorCanBeHovered;
    /** Holds restricted anchor roles. */
    QSet<QString> m_restrictedAnchorRoles;
    /** Holds currently hovered anchor. */
    QString m_strHoveredAnchor;
};

/** Rich text string implementation which parses the passed QString
  * and holds it as the tree of the formatted rich text blocks.
  * @todo: To be moved into separate files. */
class UIRichTextString
{
public:

    /** Rich text block types. */
    enum Type
    {
        Type_None,
        Type_Anchor,
        Type_Bold,
        Type_Italic,
    };

    /** Default (empty) constructor. */
    UIRichTextString(Type type = Type_None);

    /** Constructor taking passed QString.
      * @param strString     holds the QString being parsed and held as tree of rich text blocks,
      * @param type          holds the type of <i>this</i> rich text block,
      * @param strStringMeta holds the QString containing meta data describing <i>this</i> rich text block. */
    UIRichTextString(const QString &strString, Type type = Type_None, const QString &strStringMeta = QString());

    /** Destructor. */
    ~UIRichTextString();

    /** Returns the QString representation. */
    QString toString() const;

    /** Returns the list of existing format ranges appropriate for QTextLayout.
      * @param iShift holds the shift of <i>this</i> rich text block accordig to it's parent. */
    QList<QTextLayout::FormatRange> formatRanges(int iShift = 0) const;

    /** Defines the hovered anchor for <i>this</i> rich text block. */
    void setHoveredAnchor(const QString &strHoveredAnchor);

private:

    /** Parses the string. */
    void parse();

    /** Used to populate const static map of known patterns.
      * @note Keep it sync with the method below - #populatePatternHasMeta(). */
    static QMap<Type, QString> populatePatterns();
    /** Used to populate const static map of meta flags for the known patterns.
      * @note Keep it sync with the method above - #populatePatterns(). */
    static QMap<Type, bool> populatePatternHasMeta();

    /** Recursively searching for the maximum level of the passed pattern.
      * @param strString         holds the string to check for the current (recursively advanced) pattern in,
      * @param strPattern        holds the etalon pattern to recursively advance the current pattern with,
      * @param strCurrentPattern holds the current (recursively advanced) pattern to check for the presence of,
      * @param iCurrentLevel     holds the current level of the recursively advanced pattern. */
    static int searchForMaxLevel(const QString &strString, const QString &strPattern,
                                 const QString &strCurrentPattern, int iCurrentLevel = 0);

    /** Recursively composing the pattern of the maximum level.
      * @param strPattern        holds the etalon pattern to recursively update the current pattern with,
      * @param strCurrentPattern holds the current (recursively advanced) pattern,
      * @param iCurrentLevel     holds the amount of the levels left to recursively advance current pattern. */
    static QString composeFullPattern(const QString &strPattern,
                                      const QString &strCurrentPattern, int iCurrentLevel);

    /** Composes the QTextCharFormat correpoding to passed @a type. */
    static QTextCharFormat textCharFormat(Type type);

    /** Holds the type of <i>this</i> rich text block. */
    Type m_type;
    /** Holds the string of <i>this</i> rich text block. */
    QString m_strString;
    /** Holds the string meta data of <i>this</i> rich text block. */
    QString m_strStringMeta;
    /** Holds the children of <i>this</i> rich text block. */
    QMap<int, UIRichTextString*> m_strings;

    /** Holds the anchor of <i>this</i> rich text block. */
    QString m_strAnchor;
    /** Holds the anchor to highlight in <i>this</i> rich text block and in it's children. */
    QString m_strHoveredAnchor;

    /** Holds the 'any' string pattern. */
    static const QString m_sstrAny;
    /** Holds the map of known patterns. */
    static const QMap<Type, QString> m_sPatterns;
    /** Holds the map of meta flags for the known patterns. */
    static const QMap<Type, bool> m_sPatternHasMeta;
};

#endif /* !___UIGraphicsTextPane_h___ */

