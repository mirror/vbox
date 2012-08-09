/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGChooserItemGroup class declaration
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

#ifndef __UIGChooserItemGroup_h__
#define __UIGChooserItemGroup_h__

/* GUI includes: */
#include "UIGChooserItem.h"

/* Forward declarations: */
class QGraphicsScene;
class UIGraphicsButton;
class UIGraphicsRotatorButton;
class QLineEdit;
class QGraphicsProxyWidget;

/* Graphics group item
 * for graphics selector model/view architecture: */
class UIGChooserItemGroup : public UIGChooserItem
{
    Q_OBJECT;
    Q_PROPERTY(int additionalHeight READ additionalHeight WRITE setAdditionalHeight);

signals:

    /* Notifiers: Toggle stuff: */
    void sigToggleStarted();
    void sigToggleFinished();

public:

    /* Class-name used for drag&drop mime-data format: */
    static QString className();

    /* Graphics-item type: */
    enum { Type = UIGChooserItemType_Group };
    int type() const { return Type; }

    /* Constructor/destructor: */
    UIGChooserItemGroup(QGraphicsScene *pScene);
    UIGChooserItemGroup(QGraphicsScene *pScene, UIGChooserItemGroup *pCopyFrom,
                        bool fMainRoot);
    UIGChooserItemGroup(UIGChooserItem *pParent, const QString &strName,
                        bool fOpened  = false , int iPosition  = -1 );
    UIGChooserItemGroup(UIGChooserItem *pParent, UIGChooserItemGroup *pCopyFrom,
                        int iPosition  = -1 );
    ~UIGChooserItemGroup();

    /* API: Basic stuff: */
    QString name() const;
    bool closed() const;
    bool opened() const;
    void close(bool fAnimated = true);
    void open(bool fAnimated = true);

    /* API: Children stuff: */
    bool contains(const QString &strId, bool fRecursively = false) const;

private slots:

    /* Handler: Group name editing: */
    void sltNameEditingFinished();

    /* Handler: Collapse/expand stuff: */
    void sltGroupToggleStart();
    void sltGroupToggleFinish(bool fToggled);

    /* Handler: Indent root stuff: */
    void sltIndentRoot();
    void sltUnindentRoot();

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
        GroupItemData_ToggleButtonPixmap,
        GroupItemData_EnterButtonPixmap,
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
        GroupItemData_ToggleButtonSize,
        GroupItemData_EnterButtonSize,
        GroupItemData_ExitButtonSize,
        GroupItemData_MinimumNameSize,
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

    /* Helpers: Basic stuff: */
    void show();
    void hide();
    void startEditing();
    bool isMainRoot() const { return m_fMainRoot; }

    /* Helpers: Children stuff: */
    void addItem(UIGChooserItem *pItem, int iPosition);
    void removeItem(UIGChooserItem *pItem);
    void setItems(const QList<UIGChooserItem*> &items, UIGChooserItemType type);
    QList<UIGChooserItem*> items(UIGChooserItemType type = UIGChooserItemType_Any) const;
    bool hasItems(UIGChooserItemType type = UIGChooserItemType_Any) const;
    void clearItems(UIGChooserItemType type = UIGChooserItemType_Any);

    /* Helpers: Layout stuff: */
    void updateSizeHint();
    void updateLayout();
    int minimumWidthHint(bool fClosedGroup) const;
    int minimumHeightHint(bool fClosedGroup) const;
    int minimumWidthHint() const;
    int minimumHeightHint() const;
    QSizeF minimumSizeHint(bool fClosedGroup) const;
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;

    /* Helpers: Drag and drop stuff: */
    QPixmap toPixmap();
    bool isDropAllowed(QGraphicsSceneDragDropEvent *pEvent, DragToken where) const;
    void processDrop(QGraphicsSceneDragDropEvent *pEvent, UIGChooserItem *pFromWho, DragToken where);
    void resetDragToken();
    QMimeData* createMimeData();

    /* Event handlers: */
    void hoverMoveEvent(QGraphicsSceneHoverEvent *pEvent);

    /* Helpers: Paint stuff: */
    void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, QWidget *pWidget = 0);
    void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, bool fClosedGroup);
    void paintDecorations(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption);
    void paintBackground(QPainter *pPainter, const QRect &rect);
    void paintGroupInfo(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, bool fClosedGroup);

    /* Helpers: Animation stuff: */
    void updateAnimationParameters();
    void setAdditionalHeight(int iAdditionalHeight);
    int additionalHeight() const;

    /* Helpers: Prepare stuff: */
    void prepare();
    static void copyContent(UIGChooserItemGroup *pFrom, UIGChooserItemGroup *pTo);

    /* Variables: */
    QString m_strName;
    bool m_fClosed;
    UIGraphicsRotatorButton *m_pToggleButton;
    UIGraphicsButton *m_pEnterButton;
    UIGraphicsButton *m_pExitButton;
    QLineEdit *m_pNameEditorWidget;
    QGraphicsProxyWidget *m_pNameEditor;
    QList<UIGChooserItem*> m_groupItems;
    QList<UIGChooserItem*> m_machineItems;
    int m_iAdditionalHeight;
    int m_iCornerRadius;
    bool m_fMainRoot;
};

#endif /* __UIGChooserItemGroup_h__ */

