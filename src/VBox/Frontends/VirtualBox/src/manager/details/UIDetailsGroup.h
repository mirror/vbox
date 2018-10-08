/* $Id$ */
/** @file
 * VBox Qt GUI - UIDetailsGroup class declaration.
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

#ifndef ___UIDetailsGroup_h___
#define ___UIDetailsGroup_h___

/* GUI includes: */
#include "UIDetailsItem.h"

/* Forward declarations: */
class QGraphicsScene;
class UIVirtualMachineItem;

/** UIDetailsItem extension implementing group item. */
class UIDetailsGroup : public UIDetailsItem
{
    Q_OBJECT;

signals:

    /** @name Layout stuff.
      * @{ */
        /** Notifies listeners about @a iMinimumWidthHint changed. */
        void sigMinimumWidthHintChanged(int iMinimumWidthHint);
        /** Notifies listeners about @a iMinimumHeightHint changed. */
        void sigMinimumHeightHintChanged(int iMinimumHeightHint);
    /** @} */

public:

    /** RTTI item type. */
    enum { Type = UIDetailsItemType_Group };

    /** Constructs group item, passing pScene to the base-class. */
    UIDetailsGroup(QGraphicsScene *pScene);
    /** Destructs group item. */
    virtual ~UIDetailsGroup() /* override */;

    /** @name Item stuff.
      * @{ */
        /** Builds group based on passed @a machineItems. */
        void buildGroup(const QList<UIVirtualMachineItem*> &machineItems);
        /** Builds group based on cached machine items. */
        void rebuildGroup();
        /** Stops currently building group. */
        void stopBuildingGroup();
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Returns children items of certain @a enmType. */
        virtual QList<UIDetailsItem*> items(UIDetailsItemType enmType = UIDetailsItemType_Set) const /* override */;
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Updates layout. */
        virtual void updateLayout() /* override */;

        /** Returns minimum width-hint. */
        virtual int minimumWidthHint() const /* override */;
        /** Returns minimum height-hint. */
        virtual int minimumHeightHint() const /* override */;
    /** @} */

protected slots:

    /** @name Item stuff.
      * @{ */
        /** Handles request about starting step build.
          * @param  strStepId    Brings the step ID.
          * @param  iStepNumber  Brings the step number. */
    /** @} */
    virtual void sltBuildStep(QString strStepId, int iStepNumber) /* override */;

protected:

    /** @name Event-handling stuff.
      * @{ */
        /** Performs painting using passed @a pPainter, @a pOptions and optionally specified @a pWidget. */
        virtual void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions, QWidget *pWidget = 0) /* override */;
    /** @} */

    /** @name Item stuff.
      * @{ */
        /** Returns RTTI item type. */
        virtual int type() const /* override */ { return Type; }

        /** Returns the description of the item. */
        virtual QString description() const /* override */ { return QString(); }
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Adds child @a pItem. */
        virtual void addItem(UIDetailsItem *pItem) /* override */;
        /** Removes child @a pItem. */
        virtual void removeItem(UIDetailsItem *pItem) /* override */;

        /** Returns whether there are children items of certain @a enmType. */
        virtual bool hasItems(UIDetailsItemType enmType = UIDetailsItemType_Set) const /* override */;
        /** Clears children items of certain @a enmType. */
        virtual void clearItems(UIDetailsItemType enmType = UIDetailsItemType_Set) /* override */;
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Updates geometry. */
        virtual void updateGeometry() /* override */;
    /** @} */

private:

    /** @name Prepare/cleanup cascade.
      * @{ */
        /** Prepares connections. */
        void prepareConnections();
    /** @} */

    /** @name Painting stuff.
      * @{ */
        /** Paints background using specified @a pPainter and certain @a pOptions. */
        void paintBackground(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions) const;
    /** @} */

    /** @name Item stuff.
      * @{ */
        /** Holds the build step instance. */
        UIPrepareStep *m_pBuildStep;
        /** Holds the generated group ID. */
        QString        m_strGroupId;
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Holds the cached machine item list. */
        QList<UIVirtualMachineItem*> m_machineItems;

        /** Holds the child list (a list of sets). */
        QList<UIDetailsItem*> m_items;
    /** @} */

    /** @name Layout stuff.
      * @{ */
        int m_iPreviousMinimumWidthHint;
        int m_iPreviousMinimumHeightHint;
    /** @} */
};

#endif /* !___UIDetailsGroup_h___ */
