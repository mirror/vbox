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

public:

    /* Class-name used for drag&drop mime-data format: */
    static QString className();

    /* Graphics-item type: */
    enum { Type = UIGSelectorItemType_Group };
    int type() const { return Type; }

    /* Constructor/destructor: */
    UIGSelectorItemGroup(UIGSelectorItem *pParent, const QString &strName, int iPosition = -1);
    UIGSelectorItemGroup(UIGSelectorItem *pParent, UIGSelectorItemGroup *pCopyFrom, int iPosition = -1);
    ~UIGSelectorItemGroup();

    /* API: Basic stuff: */
    QString name() const;
    void setName(const QString &strName);
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
    void sltGroupToggled(bool fAnimated);

    /* Slot to handle collapse/expand animation: */
    void sltHandleResizeAnimationProgress(const QVariant &value);

private:

    /* Data enumerator: */
    enum GroupItemData
    {
        /* Layout hints: */
        GroupItemData_GroupItemMargin,
        GroupItemData_GroupItemMajorSpacing,
        GroupItemData_GroupItemMinorSpacing,
        /* Fonts: */
        GroupItemData_GroupNameFont,
        /* Sizes: */
        GroupItemData_GroupButtonSize,
        GroupItemData_GroupNameSize,
        GroupItemData_GroupNameEditorSize,
        /* Pixmaps: */
        GroupItemData_GroupPixmap,
        /* Text: */
        GroupItemData_GroupName
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
    bool hasItems(UIGSelectorItemType type = UIGSelectorItemType_Any) const;
    void clearItems(UIGSelectorItemType type = UIGSelectorItemType_Any);

    /* Layout stuff: */
    void updateSizeHint();
    void updateLayout();
    void setDesiredWidth(int iDesiredWidth);
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;
    QSizeF sizeHint(Qt::SizeHint which, bool fClosedGroup) const;

    /* API: Drag and drop stuff: */
    QPixmap toPixmap();
    bool isDropAllowed(QGraphicsSceneDragDropEvent *pEvent, DragToken where) const;
    void processDrop(QGraphicsSceneDragDropEvent *pEvent, UIGSelectorItem *pFromWho, DragToken where);
    void resetDragToken();
    QMimeData* createMimeData();

    /* Paint stuff: */
    void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, QWidget *pWidget = 0);
    void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, bool fClosedGroup);

    /* Animation stuff: */
    void setAdditionalHeight(int iAdditionalHeight);
    int additionalHeight() const;

    /* Helpers: */
    void prepare();
    static void copyContent(UIGSelectorItemGroup *pFrom, UIGSelectorItemGroup *pTo);

    /* Variables: */
    QString m_strName;
    bool m_fClosed;
    UIGraphicsRotatorButton *m_pToggleButton;
    QLineEdit *m_pNameEditorWidget;
    QGraphicsProxyWidget *m_pNameEditor;
    QPointer<QPropertyAnimation> m_pResizeAnimation;
    QList<UIGSelectorItem*> m_groupItems;
    QList<UIGSelectorItem*> m_machineItems;
    int m_iAdditionalHeight;
};

#endif // __UIGSelectorItemGroup_h__

