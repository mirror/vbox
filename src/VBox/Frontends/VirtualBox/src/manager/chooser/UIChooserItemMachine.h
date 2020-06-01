/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserItemMachine class declaration.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_manager_chooser_UIChooserItemMachine_h
#define FEQT_INCLUDED_SRC_manager_chooser_UIChooserItemMachine_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIChooserItem.h"
#include "UIManagerDefs.h"

/* Forward declarations: */
class UIChooserNodeMachine;
class UIVirtualMachineItem;


/** UIChooserItem extension implementing machine item. */
class UIChooserItemMachine : public UIChooserItem
{
    Q_OBJECT;

public:

    /** RTTI required for qgraphicsitem_cast. */
    enum { Type = UIChooserNodeType_Machine };

    /** Build item for certain @a pNode, passing @a pParent to the base-class. */
    UIChooserItemMachine(UIChooserItem *pParent, UIChooserNodeMachine *pNode);
    /** Destructs machine item. */
    virtual ~UIChooserItemMachine() /* override */;

    /** @name Item stuff.
      * @{ */
        /** Returns machine node reference. */
        UIChooserNodeMachine *nodeToMachineType() const;
        /** Returns item machine id. */
        QUuid id() const;
        /** Returns whether item accessible. */
        bool accessible() const;

        /** Returns virtual machine cache instance. */
        UIVirtualMachineItem *cache() const;
        /** Returns virtual machine cache type. */
        UIVirtualMachineItemType cacheType() const;

        /** Recaches item contents. */
        void recache();

        /** Returns whether VM is locked. */
        bool isLockedMachine() const;

        /** Returns whether passed @a position belongs to tool button area. */
        bool isToolButtonArea(const QPoint &position, int iMarginMultiplier = 1) const;
    /** @} */

    /** @name Navigation stuff.
      * @{ */
        /** Class-name used for drag&drop mime-data format. */
        static QString className();

        /** Enumerates machine items from @a il to @a ol using @a iEnumerationFlags. */
        static void enumerateMachineItems(const QList<UIChooserItem*> &il,
                                          QList<UIChooserItemMachine*> &ol,
                                          int iEnumerationFlags = 0);
    /** @} */

protected:

    /** @name Event-handling stuff.
      * @{ */
        /** Handles translation event. */
        virtual void retranslateUi() /* override */;

        /** Handles show @a pEvent. */
        virtual void showEvent(QShowEvent *pEvent) /* override */;

        /** Handles resize @a pEvent. */
        virtual void resizeEvent(QGraphicsSceneResizeEvent *pEvent) /* override */;

        /** Handles mouse press @a pEvent. */
        virtual void mousePressEvent(QGraphicsSceneMouseEvent *pEvent) /* override */;

        /** Performs painting using passed @a pPainter, @a pOptions and optionally specified @a pWidget. */
        virtual void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions, QWidget *pWidget = 0) /* override */;
    /** @} */

    /** @name Item stuff.
      * @{ */
        /** Returns RTTI item type. */
        virtual int type() const /* override */ { return Type; }

        /** Starts item editing. */
        virtual void startEditing() /* override */;

        /** Updates item. */
        virtual void updateItem() /* override */;
        /** Updates item tool-tip. */
        virtual void updateToolTip() /* override */;
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Returns children items of certain @a enmType. */
        virtual QList<UIChooserItem*> items(UIChooserNodeType enmType = UIChooserNodeType_Any) const /* override */;

        /** Adds possible @a fFavorite child @a pItem to certain @a iPosition. */
        virtual void addItem(UIChooserItem *pItem, bool fFavorite, int iPosition) /* override */;
        /** Removes child @a pItem. */
        virtual void removeItem(UIChooserItem *pItem) /* override */;

        /** Searches for a first child item answering to specified @a strSearchTag and @a iSearchFlags. */
        virtual UIChooserItem *searchForItem(const QString &strSearchTag, int iSearchFlags) /* override */;

        /** Searches for a first machine child item. */
        virtual UIChooserItem *firstMachineItem() /* override */;
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Updates layout. */
        virtual void updateLayout() /* override */;

        /** Returns minimum width-hint. */
        virtual int minimumWidthHint() const /* override */;
        /** Returns minimum height-hint. */
        virtual int minimumHeightHint() const /* override */;

        /** Returns size-hint.
          * @param  enmWhich    Brings size-hint type.
          * @param  constraint  Brings size constraint. */
        virtual QSizeF sizeHint(Qt::SizeHint enmWhich, const QSizeF &constraint = QSizeF()) const /* override */;
    /** @} */

    /** @name Navigation stuff.
      * @{ */
        /** Returns pixmap item representation. */
        virtual QPixmap toPixmap() /* override */;

        /** Returns whether item drop is allowed.
          * @param  pEvent    Brings information about drop event.
          * @param  enmPlace  Brings the place of drag token to the drop moment. */
        virtual bool isDropAllowed(QGraphicsSceneDragDropEvent *pEvent, UIChooserItemDragToken where) const /* override */;
        /** Processes item drop.
          * @param  pEvent    Brings information about drop event.
          * @param  pFromWho  Brings the item according to which we choose drop position.
          * @param  enmPlace  Brings the place of drag token to the drop moment (according to item mentioned above). */
        virtual void processDrop(QGraphicsSceneDragDropEvent *pEvent, UIChooserItem *pFromWho, UIChooserItemDragToken where) /* override */;
        /** Reset drag token. */
        virtual void resetDragToken() /* override */;

        /** Returns D&D mime data. */
        virtual QMimeData *createMimeData() /* override */;
    /** @} */

private slots:

    /** @name Item stuff.
      * @{ */
        /** Handles top-level window remaps. */
        void sltHandleWindowRemapped();

        /** Updates first row maximum width. */
        void sltUpdateFirstRowMaximumWidth() { updateFirstRowMaximumWidth(); }
    /** @} */

private:

    /** Data field types. */
    enum MachineItemData
    {
        /* Layout hints: */
        MachineItemData_MarginHL,
        MachineItemData_MarginHR,
        MachineItemData_MarginV,
        MachineItemData_MajorSpacing,
        MachineItemData_MinorSpacing,
        MachineItemData_TextSpacing,
        MachineItemData_ButtonMargin,
    };

    /** @name Prepare/cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Cleanups all. */
        void cleanup();
    /** @} */

    /** @name Item stuff.
      * @{ */
        /** Returns abstractly stored data value for certain @a iKey. */
        QVariant data(int iKey) const;
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Updates pixmaps. */
        void updatePixmaps();
        /** Updates pixmap. */
        void updatePixmap();
        /** Updates state pixmap. */
        void updateStatePixmap();
        /** Updates tool pixmap. */
        void updateToolPixmap();
        /** Updates first row maximum width. */
        void updateFirstRowMaximumWidth();
        /** Updates minimum name width. */
        void updateMinimumNameWidth();
        /** Updates minimum snapshot name width. */
        void updateMinimumSnapshotNameWidth();
        /** Updates maximum name width. */
        void updateMaximumNameWidth();
        /** Updates maximum snapshot name width. */
        void updateMaximumSnapshotNameWidth();
        /** Updates visible name. */
        void updateVisibleName();
        /** Updates visible snapshot name. */
        void updateVisibleSnapshotName();
        /** Updates state text size. */
        void updateStateTextSize();
    /** @} */

    /** @name Painting stuff.
      * @{ */
        /** Paints background using specified @a pPainter and certain @a rectangle. */
        void paintBackground(QPainter *pPainter, const QRect &rectangle) const;
        /** Paints frame using specified @a pPainter and certain @a rectangle. */
        void paintFrame(QPainter *pPainter, const QRect &rectangle) const;
        /** Paints machine info using specified @a pPainter and certain @a rectangle. */
        void paintMachineInfo(QPainter *pPainter, const QRect &rectangle) const;
    /** @} */

    /** @name Navigation stuff.
      * @{ */
        /** Returns whether machine items @a list contains passed @a pItem. */
        static bool checkIfContains(const QList<UIChooserItemMachine*> &list,
                                    UIChooserItemMachine *pItem);
    /** @} */

    /** @name Item stuff.
      * @{ */
        /** Holds item minimum default lightness. */
        int  m_iDefaultLightnessMin;
        /** Holds item maximum default lightness. */
        int  m_iDefaultLightnessMax;
        /** Holds item minimum hover lightness. */
        int  m_iHoverLightnessMin;
        /** Holds item maximum hover lightness. */
        int  m_iHoverLightnessMax;
        /** Holds item minimum highlight lightness. */
        int  m_iHighlightLightnessMin;
        /** Holds item maximum highlight lightness. */
        int  m_iHighlightLightnessMax;

        /** Holds item pixmap. */
        QPixmap  m_pixmap;
        /** Holds item state pixmap. */
        QPixmap  m_statePixmap;
        /** Holds item tool pixmap. */
        QPixmap  m_toolPixmap;

        /** Holds item visible name. */
        QString  m_strVisibleName;
        /** Holds item visible snapshot name. */
        QString  m_strVisibleSnapshotName;

        /** Holds item name font. */
        QFont  m_nameFont;
        /** Holds item snapshot name font. */
        QFont  m_snapshotNameFont;
        /** Holds item state text font. */
        QFont  m_stateTextFont;
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Holds pixmap size. */
        QSize  m_pixmapSize;
        /** Holds state pixmap size. */
        QSize  m_statePixmapSize;
        /** Holds tool pixmap size. */
        QSize  m_toolPixmapSize;
        /** Holds visible name size. */
        QSize  m_visibleNameSize;
        /** Holds visible snapshot name size. */
        QSize  m_visibleSnapshotNameSize;
        /** Holds state text size. */
        QSize  m_stateTextSize;

        /** Holds first row maximum width. */
        int  m_iFirstRowMaximumWidth;
        /** Holds minimum name width. */
        int  m_iMinimumNameWidth;
        /** Holds maximum name width. */
        int  m_iMaximumNameWidth;
        /** Holds minimum snapshot name width. */
        int  m_iMinimumSnapshotNameWidth;
        /** Holds maximum snapshot name width. */
        int  m_iMaximumSnapshotNameWidth;
    /** @} */
};


#endif /* !FEQT_INCLUDED_SRC_manager_chooser_UIChooserItemMachine_h */
