/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGSelectorItemGroup class declaration
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

#ifndef __UIGSelectorItemGroup_h__
#define __UIGSelectorItemGroup_h__

/* Qt includes: */
#include <QPointer>

/* GUI includes: */
#include "UIGSelectorItem.h"

/* Forward declarations: */
class UIGSelectorItemMachine;
class UIGraphicsRotatorButton;
class QLineEdit;
class QGraphicsProxyWidget;

/* Graphics group item
 * for graphics selector model/view architecture: */
class UIGSelectorItemGroup : public UIGSelectorItem
{
    Q_OBJECT;
    Q_PROPERTY(int additionalHeight READ additionalHeight WRITE setAdditionalHeight);

public slots:

    /* API: Loading stuff: */
    void sltOpen();

public:

    /* Class-name used for drag&drop mime-data format: */
    static QString className();

    /* Graphics-item type: */
    enum { Type = UIGSelectorItemType_Group };
    int type() const { return Type; }

    /* Constructor/destructor: */
    UIGSelectorItemGroup(UIGSelectorItem *pParent = 0, const QString &strName = QString(), int iPosition = -1);
    UIGSelectorItemGroup(UIGSelectorItem *pParent, UIGSelectorItemGroup *pCopyFrom, int iPosition = -1);
    ~UIGSelectorItemGroup();

    /* API: Basic stuff: */
    QString name() const;
    bool closed() const;
    bool opened() const;
    void close();
    void open();

    /* API: Children stuff: */
    bool contains(const QString &strId, bool fRecursively = false) const;

private slots:

    /* Slot to handle group name editing: */
    void sltNameEditingFinished();

    /* Slot to handle group collapse/expand: */
    void sltGroupToggleStart();
    void sltGroupToggleFinish(bool fToggled);

private:

    /* Data enumerator: */
    enum GroupItemData
    {
        /* Layout hints: */
        GroupItemData_HorizonalMargin,
        GroupItemData_VerticalMargin,
        GroupItemData_MajorSpacing,
        GroupItemData_MinorSpacing,
        /* Pixmaps: */
        GroupItemData_ButtonPixmap,
        GroupItemData_GroupPixmap,
        GroupItemData_MachinePixmap,
        /* Fonts: */
        GroupItemData_NameFont,
        GroupItemData_InfoFont,
        /* Text: */
        GroupItemData_Name,
        GroupItemData_GroupCountText,
        GroupItemData_MachineCountText,
        /* Sizes: */
        GroupItemData_ButtonSize,
        GroupItemData_NameSize,
        GroupItemData_NameEditorSize,
        GroupItemData_GroupPixmapSize,
        GroupItemData_MachinePixmapSize,
        GroupItemData_GroupCountTextSize,
        GroupItemData_MachineCountTextSize,
        GroupItemData_FullHeaderSize
    };

    /* Data provider: */
    QVariant data(int iKey) const;

    /* Basic stuff: */
    void startEditing();

    /* Children stuff: */
    void addItem(UIGSelectorItem *pItem, int iPosition);
    void removeItem(UIGSelectorItem *pItem);
    void moveItemTo(UIGSelectorItem *pItem, int iPosition);
    QList<UIGSelectorItem*> items(UIGSelectorItemType type = UIGSelectorItemType_Any) const;
    bool hasItems(UIGSelectorItemType type = UIGSelectorItemType_Any) const;
    void clearItems(UIGSelectorItemType type = UIGSelectorItemType_Any);

    /* Layout stuff: */
    void updateSizeHint();
    void updateLayout();
    int minimumWidthHint(bool fClosedGroup) const;
    int minimumHeightHint(bool fClosedGroup) const;
    int minimumWidthHint() const;
    int minimumHeightHint() const;
    QSizeF minimumSizeHint(bool fClosedGroup) const;
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;

    /* API: Drag and drop stuff: */
    QPixmap toPixmap();
    bool isDropAllowed(QGraphicsSceneDragDropEvent *pEvent, DragToken where) const;
    void processDrop(QGraphicsSceneDragDropEvent *pEvent, UIGSelectorItem *pFromWho, DragToken where);
    void resetDragToken();
    QMimeData* createMimeData();

    /* Paint stuff: */
    void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, QWidget *pWidget = 0);
    void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, bool fClosedGroup);
    void paintDecorations(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption);
    void paintGroupInfo(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, bool fClosedGroup);

    /* Animation stuff: */
    void updateAnimationParameters();
    void setAdditionalHeight(int iAdditionalHeight);
    int additionalHeight() const;

    /* Helpers: */
    void prepare();
    static void copyContent(UIGSelectorItemGroup *pFrom, UIGSelectorItemGroup *pTo);

    /* Variables: */
    QString m_strName;
    bool m_fClosed;
    UIGraphicsRotatorButton *m_pButton;
    QLineEdit *m_pNameEditorWidget;
    QGraphicsProxyWidget *m_pNameEditor;
    QList<UIGSelectorItem*> m_groupItems;
    QList<UIGSelectorItem*> m_machineItems;
    int m_iAdditionalHeight;
    int m_iCornerRadius;
};

#endif /* __UIGSelectorItemGroup_h__ */

