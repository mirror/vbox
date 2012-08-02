/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGSelectorItemMachine class declaration
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

#ifndef __UIGSelectorItemMachine_h__
#define __UIGSelectorItemMachine_h__

/* GUI includes: */
#include "UIVMItem.h"
#include "UIGSelectorItem.h"

/* Forward declarations: */
class CMachine;

/* Graphics machine item
 * for graphics selector model/view architecture: */
class UIGSelectorItemMachine : public UIGSelectorItem, public UIVMItem
{
    Q_OBJECT;

public:

    /* Class-name used for drag&drop mime-data format: */
    static QString className();

    /* Graphics-item type: */
    enum { Type = UIGSelectorItemType_Machine };
    int type() const { return Type; }

    /* Constructor/destructor: */
    UIGSelectorItemMachine(UIGSelectorItem *pParent, const CMachine &machine, int iPosition = -1);
    UIGSelectorItemMachine(UIGSelectorItem *pParent, UIGSelectorItemMachine *pCopyFrom, int iPosition = -1);
    ~UIGSelectorItemMachine();

    /* API: Basic stuff: */
    QString name() const;

private:

    /* Data enumerator: */
    enum MachineItemData
    {
        /* Layout hints: */
        MachineItemData_MachineItemMargin,
        MachineItemData_MachineItemMajorSpacing,
        MachineItemData_MachineItemMinorSpacing,
        MachineItemData_MachineItemTextSpacing,
        /* Fonts: */
        MachineItemData_MachineNameFont,
        MachineItemData_SnapshotNameFont,
        MachineItemData_MachineStateTextFont,
        /* Sizes: */
        MachineItemData_MachinePixmapSize,
        MachineItemData_MachineNameSize,
        MachineItemData_SnapshotNameSize,
        MachineItemData_MachineStatePixmapSize,
        MachineItemData_MachineStateTextSize,
        /* Pixmaps: */
        MachineItemData_MachinePixmap,
        MachineItemData_MachineStatePixmap,
        /* Text: */
        MachineItemData_MachineName,
        MachineItemData_SnapshotName,
        MachineItemData_MachineStateText,
        /* Other values: */
        MachineItemData_MachineId,
        MachineItemData_MachineOSTypeId,
        MachineItemData_MachineState
    };

    /* Data provider: */
    QVariant data(int iKey) const;

    /* Basic stuff: */
    void startEditing();
    void cleanup();

    /* Children stuff: */
    void addItem(UIGSelectorItem *pItem, int iPosition);
    void removeItem(UIGSelectorItem *pItem);
    QList<UIGSelectorItem*> items(UIGSelectorItemType type) const;
    bool hasItems(UIGSelectorItemType type) const;
    void clearItems(UIGSelectorItemType type);

    /* Layout stuff: */
    void updateSizeHint();
    void updateLayout();
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;

    /* Drag and drop stuff: */
    QPixmap toPixmap();
    bool isDropAllowed(QGraphicsSceneDragDropEvent *pEvent, DragToken where) const;
    void processDrop(QGraphicsSceneDragDropEvent *pEvent, UIGSelectorItem *pFromWho, DragToken where);
    void resetDragToken();
    QMimeData* createMimeData();

    /* Paint stuff: */
    void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, QWidget *pWidget = 0);
};

#endif // __UIGSelectorItemMachine_h__

