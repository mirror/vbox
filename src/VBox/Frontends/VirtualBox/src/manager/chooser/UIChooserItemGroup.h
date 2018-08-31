/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserItemGroup class declaration.
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

#ifndef ___UIChooserItemGroup_h___
#define ___UIChooserItemGroup_h___

/* Qt includes: */
#include <QPixmap>
#include <QString>
#include <QWidget>

/* GUI includes: */
#include "UIChooserItem.h"

/* Forward declarations: */
class QGraphicsProxyWidget;
class QGraphicsScene;
class QLineEdit;
class QMenu;
class QMimeData;
class QPainter;
class QStyleOptionGraphicsItem;
class UIEditorGroupRename;
class UIGraphicsButton;
class UIGraphicsRotatorButton;


/** UIChooserItem extension implementing group item. */
class UIChooserItemGroup : public UIChooserItem
{
    Q_OBJECT;
    Q_PROPERTY(int additionalHeight READ additionalHeight WRITE setAdditionalHeight);

signals:

    /** @name Item stuff.
      * @{ */
        /** Notifies listeners about toggle start. */
        void sigToggleStarted();
        /** Notifies listeners about toggle finish. */
        void sigToggleFinished();
    /** @} */

public:

    /** RTTI item type. */
    enum { Type = UIChooserItemType_Group };

    /** Constructs main-root item, passing pScene to the base-class. */
    UIChooserItemGroup(QGraphicsScene *pScene);
    /** Constructs temporary @a fMainRoot item as a @a pCopyFrom, passing pScene to the base-class. */
    UIChooserItemGroup(QGraphicsScene *pScene, UIChooserItemGroup *pCopyFrom, bool fMainRoot);
    /** Constructs non-root item with specified @a strName and @a iPosition, @a fOpened if requested, passing pParent to the base-class. */
    UIChooserItemGroup(UIChooserItem *pParent, const QString &strName, bool fOpened = false, int iPosition  = -1);
    /** Constructs temporary non-root item with specified @a iPosition as a @a pCopyFrom, passing pParent to the base-class. */
    UIChooserItemGroup(UIChooserItem *pParent, UIChooserItemGroup *pCopyFrom, int iPosition = -1);
    /** Destructs group item. */
    virtual ~UIChooserItemGroup() /* override */;

    /** @name Item stuff.
      * @{ */
        /** Defines group @a strName. */
        void setName(const QString &strName);

        /** Closes group in @a fAnimated way if requested. */
        void close(bool fAnimated = true);
        /** Returns whether group is closed. */
        bool isClosed() const;

        /** Opens group in @a fAnimated way if requested. */
        void open(bool fAnimated = true);
        /** Returns whether group is opened. */
        bool isOpened() const;
    /** @} */

    /** @name Navigation stuff.
      * @{ */
        /** Class-name used for drag&drop mime-data format. */
        static QString className();
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

        /** Handles hover enter @a event. */
        virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *pEvent) /* override */;
        /** Handles hover leave @a event. */
        virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *pEvent) /* override */;

        /** Performs painting using passed @a pPainter, @a pOptions and optionally specified @a pWidget. */
        virtual void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions, QWidget *pWidget = 0) /* override */;
    /** @} */

    /** @name Item stuff.
      * @{ */
        /** Returns RTTI item type. */
        virtual int type() const /* override */ { return Type; }

        /** Shows item. */
        virtual void show() /* override */;
        /** Hides item. */
        virtual void hide() /* override */;

        /** Starts item editing. */
        virtual void startEditing() /* override */;

        /** Updates item tool-tip. */
        virtual void updateToolTip() /* override */;

        /** Returns item name. */
        virtual QString name() const /* override */;
        /** Returns item description. */
        virtual QString description() const /* override */;
        /** Returns item full-name. */
        virtual QString fullName() const /* override */;
        /** Returns item definition. */
        virtual QString definition() const /* override */;

        /** Handles root status change. */
        virtual void handleRootStatusChange() /* override */;
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Adds child @a pItem to certain @a iPosition. */
        virtual void addItem(UIChooserItem *pItem, int iPosition) /* override */;
        /** Removes child @a pItem. */
        virtual void removeItem(UIChooserItem *pItem) /* override */;

        /** Replaces children @a items of certain @a enmType. */
        virtual void setItems(const QList<UIChooserItem*> &items, UIChooserItemType enmType) /* override */;
        /** Returns children items of certain @a enmType. */
        virtual QList<UIChooserItem*> items(UIChooserItemType enmType = UIChooserItemType_Any) const /* override */;
        /** Returns whether there are children items of certain @a enmType. */
        virtual bool hasItems(UIChooserItemType enmType = UIChooserItemType_Any) const /* override */;
        /** Clears children items of certain @a enmType. */
        virtual void clearItems(UIChooserItemType enmType = UIChooserItemType_Any) /* override */;

        /** Updates all children items with specified @a strId. */
        virtual void updateAllItems(const QString &strId) /* override */;
        /** Removes all children items with specified @a strId. */
        virtual void removeAllItems(const QString &strId) /* override */;

        /** Searches for a first child item answering to specified @a strSearchTag and @a iItemSearchFlags. */
        virtual UIChooserItem *searchForItem(const QString &strSearchTag, int iItemSearchFlags) /* override */;

        /** Searches for a first machine child item. */
        virtual UIChooserItem *firstMachineItem() /* override */;

        /** Sorts children items. */
        virtual void sortItems() /* override */;
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
        virtual bool isDropAllowed(QGraphicsSceneDragDropEvent *pEvent, DragToken where) const /* override */;
        /** Processes item drop.
          * @param  pEvent    Brings information about drop event.
          * @param  pFromWho  Brings the item according to which we choose drop position.
          * @param  enmPlace  Brings the place of drag token to the drop moment (according to item mentioned above). */
        virtual void processDrop(QGraphicsSceneDragDropEvent *pEvent, UIChooserItem *pFromWho, DragToken where) /* override */;
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

        /** Handles name editing trigger. */
        void sltNameEditingFinished();

        /** Handles group toggle start. */
        void sltGroupToggleStart();
        /** Handles group toggle finish for group finally @a fToggled. */
        void sltGroupToggleFinish(bool fToggled);

        /** Handles root indentation. */
        void sltIndentRoot();
        /** Handles root unindentation. */
        void sltUnindentRoot();
    /** @} */

private:

    /** Data field types. */
    enum GroupItemData
    {
        /* Layout hints: */
        GroupItemData_HorizonalMargin,
        GroupItemData_VerticalMargin,
        GroupItemData_Spacing,
    };

    /** @name Prepare/cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();

        /** Copies group contents @a pFrom one item @a pTo another. */
        static void copyContent(UIChooserItemGroup *pFrom, UIChooserItemGroup *pTo);
    /** @} */

    /** @name Item stuff.
      * @{ */
        /** Returns abstractly stored data value for certain @a iKey. */
        QVariant data(int iKey) const;

        /** Returns whether group is main-root. */
        bool isMainRoot() const { return m_fMainRoot; }

        /** Returns blackout item darkness. */
        int blackoutDarkness() const { return m_iBlackoutDarkness; }

        /** Defines @a iAdditionalHeight. */
        void setAdditionalHeight(int iAdditionalHeight);
        /** Returns additional height. */
        int additionalHeight() const;

        /** Updates animation parameters. */
        void updateAnimationParameters();
        /** Updates toggle-button tool-tip. */
        void updateToggleButtonToolTip();
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Returns whether group contains machine with @a strId. */
        bool isContainsMachine(const QString &strId) const;
        /** Returns whether group contains locked machine. */
        bool isContainsLockedMachine();

        /** Updates user count info. */
        void updateItemCountInfo();
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Returns minimum width-hint depending on whether @a fGroupOpened. */
        int minimumWidthHintForGroup(bool fGroupOpened) const;
        /** Returns minimum height-hint depending on whether @a fGroupOpened. */
        int minimumHeightHintForGroup(bool fGroupOpened) const;
        /** Returns minimum size-hint depending on whether @a fGroupOpened. */
        QSizeF minimumSizeHintForProup(bool fGroupOpened) const;

        /** Updates visible name. */
        void updateVisibleName();
        /** Updates pixmaps. */
        void updatePixmaps();
        /** Updates minimum header size. */
        void updateMinimumHeaderSize();
    /** @} */

    /** @name Painting stuff.
      * @{ */
        /** Paints background using specified @a pPainter and certain @a rect. */
        void paintBackground(QPainter *pPainter, const QRect &rect);
        /** Paints header using specified @a pPainter and certain @a rect. */
        void paintHeader(QPainter *pPainter, const QRect &rect);
    /** @} */

    /** @name Item stuff.
      * @{ */
        /** Holds whether group is main-root. */
        bool  m_fMainRoot;
        /** Holds whether group is closed. */
        bool  m_fClosed;

        /** Holds aditional height. */
        int  m_iAdditionalHeight;
        /** Holds corner radious. */
        int  m_iCornerRadius;
        /** Holds blackout item darkness. */
        int  m_iBlackoutDarkness;

        /** Holds the cached name. */
        QString m_strName;
        /** Holds the cached description. */
        QString m_strDescription;
        /** Holds the cached visible name. */
        QString m_strVisibleName;

        /** Holds the name font. */
        QFont  m_nameFont;

        /** Holds the group toggle button instance. */
        UIGraphicsRotatorButton *m_pToggleButton;
        /** Holds the group enter button instance. */
        UIGraphicsButton        *m_pEnterButton;
        /** Holds the group exit button instance. */
        UIGraphicsButton        *m_pExitButton;

        /** Holds the group name editor instance. */
        UIEditorGroupRename  *m_pNameEditorWidget;
        /** Holds the group name editor proxy instance. */
        QGraphicsProxyWidget *m_pNameEditor;
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Holds the group children list. */
        QList<UIChooserItem*>  m_groupItems;
        /** Holds the global children list. */
        QList<UIChooserItem*>  m_globalItems;
        /** Holds the machine children list. */
        QList<UIChooserItem*>  m_machineItems;

        /** Holds group children pixmap. */
        QPixmap  m_groupsPixmap;
        /** Holds machine children pixmap. */
        QPixmap  m_machinesPixmap;

        /** Holds the cached group children info. */
        QString  m_strInfoGroups;
        /** Holds the cached machine children info. */
        QString  m_strInfoMachines;

        /** Holds the children info font. */
        QFont  m_infoFont;
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Holds cached visible name size. */
        QSize  m_visibleNameSize;
        /** Holds cached group children pixmap size. */
        QSize  m_pixmapSizeGroups;
        /** Holds cached machine children pixmap size. */
        QSize  m_pixmapSizeMachines;
        /** Holds cached group children info size. */
        QSize  m_infoSizeGroups;
        /** Holds cached machine children info size. */
        QSize  m_infoSizeMachines;
        /** Holds cached minimum header size. */
        QSize  m_minimumHeaderSize;
        /** Holds cached toggle button size. */
        QSize  m_toggleButtonSize;
        /** Holds cached enter button size. */
        QSize  m_enterButtonSize;
        /** Holds cached exit button size. */
        QSize  m_exitButtonSize;
    /** @} */
};


/** QWidget extension to use as group name editor. */
class UIEditorGroupRename : public QWidget
{
    Q_OBJECT;

signals:

    /** Notifies about group editing finished. */
    void sigEditingFinished();

public:

    /** Constructs group editor with initial @a strName passing @a pParent to the base-class. */
    UIEditorGroupRename(const QString &strName, UIChooserItem *pParent);

    /** Defines editor @a strText. */
    void setText(const QString &strText);
    /** Returns editor text. */
    QString text() const;

    /** Defines editor @a font. */
    void setFont(const QFont &font);

public slots:

    /** ACquires keyboard focus. */
    void setFocus();

protected:

    /** Preprocesses any Qt @a pEvent for passed @a pObject. */
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;

private:

    /** Handles context-menu @a pEvent. */
    void handleContextMenuEvent(QContextMenuEvent *pEvent);

    /** Holds the parent reference. */
    UIChooserItem *m_pParent;

    /** Holds the line-edit instance. */
    QLineEdit *m_pLineEdit;
    /** Holds the conect-menu instance. */
    QMenu     *m_pTemporaryMenu;
};


#endif /* !___UIChooserItemGroup_h___ */
