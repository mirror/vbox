/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserItemMachine class declaration.
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIChooserItemMachine_h__
#define __UIChooserItemMachine_h__

/* GUI includes: */
#include "UIVirtualMachineItem.h"
#include "UIChooserItem.h"

/* Forward declarations: */
class CMachine;
class UIGraphicsToolBar;
class UIGraphicsZoomButton;

/* Machine-item enumeration flags: */
enum UIChooserItemMachineEnumerationFlag
{
    UIChooserItemMachineEnumerationFlag_Unique       = RT_BIT(0),
    UIChooserItemMachineEnumerationFlag_Inaccessible = RT_BIT(1)
};

/* Graphics machine item
 * for graphics selector model/view architecture: */
class UIChooserItemMachine : public UIChooserItem, public UIVirtualMachineItem
{
    Q_OBJECT;

public:

    /* Class-name used for drag&drop mime-data format: */
    static QString className();

    /* Graphics-item type: */
    enum { Type = UIChooserItemType_Machine };
    int type() const { return Type; }

    /* Constructor (new item): */
    UIChooserItemMachine(UIChooserItem *pParent, const CMachine &machine, int iPosition = -1);
    /* Constructor (new item copy): */
    UIChooserItemMachine(UIChooserItem *pParent, UIChooserItemMachine *pCopyFrom, int iPosition = -1);
    /* Destructor: */
    ~UIChooserItemMachine();

    /* API: Basic stuff: */
    QString name() const;
    QString description() const;
    QString fullName() const;
    QString definition() const;
    bool isLockedMachine() const;

    /* API: Machine-item enumeration stuff: */
    static void enumerateMachineItems(const QList<UIChooserItem*> &il,
                                      QList<UIChooserItemMachine*> &ol,
                                      int iEnumerationFlags = 0);

private slots:

    /** Handles top-level window remaps. */
    void sltHandleWindowRemapped();

private:

    /* Data enumerator: */
    enum MachineItemData
    {
        /* Layout hints: */
        MachineItemData_Margin,
        MachineItemData_MajorSpacing,
        MachineItemData_MinorSpacing,
        MachineItemData_TextSpacing,
        /* Pixmaps: */
        MachineItemData_SettingsButtonPixmap,
        MachineItemData_StartButtonPixmap,
        MachineItemData_PauseButtonPixmap,
        MachineItemData_CloseButtonPixmap,
        /* Sizes: */
        MachineItemData_ToolBarSize
    };

    /* Data provider: */
    QVariant data(int iKey) const;

    /* Helpers: Update stuff: */
    void updatePixmaps();
    void updatePixmap();
    void updateStatePixmap();
    void updateName();
    void updateSnapshotName();
    void updateFirstRowMaximumWidth();
    void updateMinimumNameWidth();
    void updateMinimumSnapshotNameWidth();
    void updateMaximumNameWidth();
    void updateMaximumSnapshotNameWidth();
    void updateVisibleName();
    void updateVisibleSnapshotName();
    void updateStateText();

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
    void updateAll(const QString &strId);
    void removeAll(const QString &strId);
    UIChooserItem* searchForItem(const QString &strSearchTag, int iItemSearchFlags);
    UIChooserItemMachine* firstMachineItem();
    void sortItems();

    /* Helpers: Layout stuff: */
    void updateLayout();
    int minimumWidthHint() const;
    int minimumHeightHint() const;
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;

    /* Helpers: Drag and drop stuff: */
    QPixmap toPixmap();
    bool isDropAllowed(QGraphicsSceneDragDropEvent *pEvent, DragToken where) const;
    void processDrop(QGraphicsSceneDragDropEvent *pEvent, UIChooserItem *pFromWho, DragToken where);
    void resetDragToken();
    QMimeData* createMimeData();

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
    static bool contains(const QList<UIChooserItemMachine*> &list,
                         UIChooserItemMachine *pItem);

    /* Variables: */
    UIGraphicsToolBar *m_pToolBar;
    UIGraphicsZoomButton *m_pSettingsButton;
    UIGraphicsZoomButton *m_pStartButton;
    UIGraphicsZoomButton *m_pPauseButton;
    UIGraphicsZoomButton *m_pCloseButton;
    int m_iHighlightLightness;
    int m_iHoverLightness;
    int m_iHoverHighlightLightness;
    /* Cached values: */
    QFont m_nameFont;
    QFont m_snapshotNameFont;
    QFont m_stateTextFont;
    QPixmap m_pixmap;
    QPixmap m_statePixmap;
    QString m_strName;
    QString m_strDescription;
    QString m_strVisibleName;
    QString m_strSnapshotName;
    QString m_strVisibleSnapshotName;
    QString m_strStateText;
    QSize m_pixmapSize;
    QSize m_statePixmapSize;
    QSize m_visibleNameSize;
    QSize m_visibleSnapshotNameSize;
    QSize m_stateTextSize;
    int m_iFirstRowMaximumWidth;
    int m_iMinimumNameWidth;
    int m_iMaximumNameWidth;
    int m_iMinimumSnapshotNameWidth;
    int m_iMaximumSnapshotNameWidth;
};

#endif /* __UIChooserItemMachine_h__ */

