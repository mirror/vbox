/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserItemGlobal class declaration.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIChooserItemGlobal_h__
#define __UIChooserItemGlobal_h__

/* GUI includes: */
#include "UIChooserItem.h"

/* Forward declarations: */
class UIGraphicsToolBar;
class UIGraphicsZoomButton;

/* Machine-item enumeration flags: */
enum UIChooserItemGlobalEnumerationFlag
{
    UIChooserItemGlobalEnumerationFlag_Unique       = RT_BIT(0),
    UIChooserItemGlobalEnumerationFlag_Inaccessible = RT_BIT(1)
};

/* Graphics global-item
 * for graphics selector model/view architecture: */
class UIChooserItemGlobal : public UIChooserItem
{
    Q_OBJECT;

public:

    /* Graphics-item type: */
    enum { Type = UIChooserItemType_Global };
    int type() const { return Type; }

    /* Constructor (new item): */
    UIChooserItemGlobal(UIChooserItem *pParent, int iPosition = -1);
    /* Constructor (new item copy): */
    UIChooserItemGlobal(UIChooserItem *pParent, UIChooserItemGlobal *pCopyFrom, int iPosition = -1);
    /* Destructor: */
    ~UIChooserItemGlobal();

    /* API: Basic stuff: */
    QString name() const;
    QString description() const;
    QString fullName() const;
    QString definition() const;

private slots:

    /** Handles top-level window remaps. */
    void sltHandleWindowRemapped();

private:

    /* Data enumerator: */
    enum GlobalItemData
    {
        /* Layout hints: */
        GlobalItemData_Margin,
        GlobalItemData_Spacing,
    };

    /* Data provider: */
    QVariant data(int iKey) const;

    /* Helpers: Update stuff: */
    void updatePixmap();
    void updateMinimumNameWidth();
    void updateMaximumNameWidth();
    void updateVisibleName();

    /* Helper: Translate stuff: */
    void retranslateUi();

    /* Helpers: Basic stuff: */
    void startEditing();
    void updateToolTip();

    /* Helpers: Children stuff: */
    void addItem(UIChooserItem *pItem, int iPosition);
    void removeItem(UIChooserItem *pItem);
    void setItems(const QList<UIChooserItem*> &items, UIChooserItemType type);
    QList<UIChooserItem*> items(UIChooserItemType type) const;
    bool hasItems(UIChooserItemType type) const;
    void clearItems(UIChooserItemType type);
    void updateAllItems(const QString &strId);
    void removeAllItems(const QString &strId);
    UIChooserItem *searchForItem(const QString &strSearchTag, int iItemSearchFlags);
    UIChooserItem *firstMachineItem();
    void sortItems();

    /* Helpers: Layout stuff: */
    void updateLayout() {}
    int minimumWidthHint() const;
    int minimumHeightHint() const;
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;

    /* Helpers: Drag and drop stuff: */
    QPixmap toPixmap();
    bool isDropAllowed(QGraphicsSceneDragDropEvent *pEvent, DragToken where) const;
    void processDrop(QGraphicsSceneDragDropEvent *pEvent, UIChooserItem *pFromWho, DragToken where);
    void resetDragToken();
    QMimeData *createMimeData();

    /** Handles show @a pEvent. */
    virtual void showEvent(QShowEvent *pEvent) /* override */;

    /* Handler: Resize handling stuff: */
    void resizeEvent(QGraphicsSceneResizeEvent *pEvent);

    /* Handler: Mouse handling stuff: */
    void mousePressEvent(QGraphicsSceneMouseEvent *pEvent);

    /* Helpers: Paint stuff: */
    void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, QWidget *pWidget = 0);
    void paintDecorations(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption);
    void paintBackground(QPainter *pPainter, const QRect &rect);
    void paintFrameRectangle(QPainter *pPainter, const QRect &rect);
    void paintMachineInfo(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption);

    /* Helpers: Prepare stuff: */
    void prepare();

    /* Helper: Machine-item enumeration stuff: */
    static bool contains(const QList<UIChooserItemGlobal*> &list,
                         UIChooserItemGlobal *pItem);

    /* Variables: */
    int m_iHighlightLightness;
    int m_iHoverLightness;
    int m_iHoverHighlightLightness;
    /* Cached values: */
    QFont m_nameFont;
    QPixmap m_pixmap;
    QString m_strName;
    QString m_strDescription;
    QString m_strVisibleName;
    QSize m_pixmapSize;
    QSize m_visibleNameSize;
    int m_iMinimumNameWidth;
    int m_iMaximumNameWidth;
};

#endif /* __UIChooserItemGlobal_h__ */
