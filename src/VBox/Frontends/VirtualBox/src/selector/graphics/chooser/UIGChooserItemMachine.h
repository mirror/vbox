/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGChooserItemMachine class declaration
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIGChooserItemMachine_h__
#define __UIGChooserItemMachine_h__

/* GUI includes: */
#include "UIVMItem.h"
#include "UIGChooserItem.h"

/* Forward declarations: */
class CMachine;
class UIGraphicsToolBar;
class UIGraphicsZoomButton;

/* Machine-item enumeration flags: */
enum UIGChooserItemMachineEnumerationFlag
{
    UIGChooserItemMachineEnumerationFlag_Unique       = RT_BIT(0),
    UIGChooserItemMachineEnumerationFlag_Inaccessible = RT_BIT(1)
};

/* Graphics machine item
 * for graphics selector model/view architecture: */
class UIGChooserItemMachine : public UIGChooserItem, public UIVMItem
{
    Q_OBJECT;

public:

    /* Class-name used for drag&drop mime-data format: */
    static QString className();

    /* Graphics-item type: */
    enum { Type = UIGChooserItemType_Machine };
    int type() const { return Type; }

    /* Constructor (new item): */
    UIGChooserItemMachine(UIGChooserItem *pParent, const CMachine &machine, int iPosition = -1);
    /* Constructor (new item copy): */
    UIGChooserItemMachine(UIGChooserItem *pParent, UIGChooserItemMachine *pCopyFrom, int iPosition = -1);
    /* Destructor: */
    ~UIGChooserItemMachine();

    /* API: Basic stuff: */
    QString name() const;
    QString fullName() const;
    QString definition() const;
    bool isLockedMachine() const;

    /* API: Machine-item enumeration stuff: */
    static void enumerateMachineItems(const QList<UIGChooserItem*> &il,
                                      QList<UIGChooserItemMachine*> &ol,
                                      int iEnumerationFlags = 0);

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
        MachineItemData_Pixmap,
        MachineItemData_StatePixmap,
        MachineItemData_SettingsButtonPixmap,
        MachineItemData_StartButtonPixmap,
        MachineItemData_PauseButtonPixmap,
        MachineItemData_CloseButtonPixmap,
        /* Fonts: */
        MachineItemData_NameFont,
        MachineItemData_SnapshotNameFont,
        MachineItemData_StateTextFont,
        /* Text: */
        MachineItemData_Name,
        MachineItemData_SnapshotName,
        MachineItemData_StateText,
        /* Sizes: */
        MachineItemData_PixmapSize,
        MachineItemData_StatePixmapSize,
        MachineItemData_NameSize,
        MachineItemData_MinimumNameWidth,
        MachineItemData_MaximumNameWidth,
        MachineItemData_SnapshotNameSize,
        MachineItemData_MinimumSnapshotNameWidth,
        MachineItemData_MaximumSnapshotNameWidth,
        MachineItemData_FirstRowMaximumWidth,
        MachineItemData_StateTextSize,
        MachineItemData_ToolBarSize
    };

    /* Data provider: */
    QVariant data(int iKey) const;

    /* Helper: Translate stuff: */
    void retranslateUi();

    /* Helpers: Basic stuff: */
    void startEditing();
    void updateToolTip();

    /* Helpers: Children stuff: */
    void addItem(UIGChooserItem *pItem, int iPosition);
    void removeItem(UIGChooserItem *pItem);
    void setItems(const QList<UIGChooserItem*> &items, UIGChooserItemType type);
    QList<UIGChooserItem*> items(UIGChooserItemType type) const;
    bool hasItems(UIGChooserItemType type) const;
    void clearItems(UIGChooserItemType type);
    void updateAll(const QString &strId);
    void removeAll(const QString &strId);
    UIGChooserItem* searchForItem(const QString &strSearchTag, int iItemSearchFlags);
    UIGChooserItemMachine* firstMachineItem();
    void sortItems();

    /* Helpers: Layout stuff: */
    void updateLayout();
    int minimumWidthHint() const;
    int minimumHeightHint() const;
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;

    /* Helpers: Drag and drop stuff: */
    QPixmap toPixmap();
    bool isDropAllowed(QGraphicsSceneDragDropEvent *pEvent, DragToken where) const;
    void processDrop(QGraphicsSceneDragDropEvent *pEvent, UIGChooserItem *pFromWho, DragToken where);
    void resetDragToken();
    QMimeData* createMimeData();

    /* Handler: Mouse stuff: */
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
    static bool contains(const QList<UIGChooserItemMachine*> &list,
                         UIGChooserItemMachine *pItem);

    /* Variables: */
    UIGraphicsToolBar *m_pToolBar;
    UIGraphicsZoomButton *m_pSettingsButton;
    UIGraphicsZoomButton *m_pStartButton;
    UIGraphicsZoomButton *m_pPauseButton;
    UIGraphicsZoomButton *m_pCloseButton;
    int m_iCornerRadius;
    int m_iHighlightLightness;
    int m_iHoverLightness;
    int m_iHoverHighlightLightness;
};

#endif /* __UIGChooserItemMachine_h__ */

