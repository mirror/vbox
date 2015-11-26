/* $Id$ */
/** @file
 * VBox Qt GUI - UIGInformationElement class declaration.
 */

/*
 * Copyright (C) 2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIGInformationElement_h__
#define __UIGInformationElement_h__

/* Qt includes: */
#include <QIcon>

/* GUI includes: */
#include "UIGInformationItem.h"
#include "UIExtraDataDefs.h"

/* Forward declarations: */
class UIGInformationSet;
class CMachine;
class UIGraphicsRotatorButton;
class UIGraphicsTextPane;
class QTextLayout;
class QStateMachine;

/* Typedefs: */
typedef QPair<QString, QString> UITextTableLine;
typedef QList<UITextTableLine> UITextTable;

/* Details element
 * for graphics details model/view architecture: */
class UIGInformationElement : public UIGInformationItem
{
    Q_OBJECT;

signals:

    /* Notifiers: Hover stuff: */
    void sigHoverLeave();

    /* Notifiers: Toggle stuff: */
    void sigToggleElement(InformationElementType type, bool fToggled);
    void sigToggleElementFinished();

public:

    /* Graphics-item type: */
    enum { Type = UIGInformationItemType_Element };
    int type() const { return Type; }

    /* Constructor/destructor: */
    UIGInformationElement(UIGInformationSet *pParent, InformationElementType type, bool fOpened);
    ~UIGInformationElement();

    /* API: Element type: */
    InformationElementType elementType() const { return m_type; }

    /* API: Update stuff: */
    virtual void updateAppearance();

protected slots:

    /** Handles children geometry changes. */
    void sltUpdateGeometry() { updateGeometry(); }

protected:

    /* Data enumerator: */
    enum ElementData
    {
        /* Hints: */
        ElementData_Margin,
        ElementData_Spacing,
        ElementData_MinimumTextColumnWidth
    };

    /** This event handler is delivered after the widget has been resized. */
    void resizeEvent(QGraphicsSceneResizeEvent *pEvent);

    /* Data provider: */
    QVariant data(int iKey) const;

    /* Helpers: Update stuff: */
    void updateMinimumHeaderWidth();
    void updateMinimumHeaderHeight();

    /* API: Icon stuff: */
    void setIcon(const QIcon &icon);

    /* API: Name stuff: */
    void setName(const QString &strName);

    /* API: Text stuff: */
    const UITextTable& text() const;
    void setText(const UITextTable &text);

    /* API: Machine stuff: */
    const CMachine& machine();

    /* Helpers: Layout stuff: */
    int minimumHeaderWidth() const { return m_iMinimumHeaderWidth; }
    int minimumHeaderHeight() const { return m_iMinimumHeaderHeight; }
    int minimumWidthHint() const;
    virtual int minimumHeightHint(bool fClosed) const;
    int minimumHeightHint() const;
    void updateLayout();

private:

    /* API: Children stuff: */
    void addItem(UIGInformationItem *pItem);
    void removeItem(UIGInformationItem *pItem);
    QList<UIGInformationItem*> items(UIGInformationItemType type) const;
    bool hasItems(UIGInformationItemType type) const;
    void clearItems(UIGInformationItemType type);

    /* Helpers: Prepare stuff: */
    void prepareElement();
    void prepareTextPane();

    /* Helpers: Paint stuff: */
    void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, QWidget *pWidget = 0);
    void paintDecorations(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption);
    void paintElementInfo(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption);
    void paintBackground(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption);

    /* Variables: */
    UIGInformationSet *m_pSet;
    InformationElementType m_type;
    QPixmap m_pixmap;
    QString m_strName;
    int m_iCornerRadius;
    QFont m_nameFont;
    QFont m_textFont;
    QSize m_pixmapSize;
    QSize m_nameSize;
    QSize m_buttonSize;
    int m_iMinimumHeaderWidth;
    int m_iMinimumHeaderHeight;

    /* Variables: Text-pane stuff: */
    UIGraphicsTextPane *m_pTextPane;

    /* Friends: */
    friend class UIGInformationSet;
};

#endif /* __UIGInformationElement_h__ */

