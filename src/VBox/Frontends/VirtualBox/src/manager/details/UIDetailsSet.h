/* $Id$ */
/** @file
 * VBox Qt GUI - UIDetailsSet class declaration.
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

#ifndef ___UIDetailsSet_h___
#define ___UIDetailsSet_h___

/* GUI includes: */
#include "UIDetailsItem.h"
#include "UIExtraDataDefs.h"
#include "UISettingsDefs.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"

/* Forward declarations: */
class UIVirtualMachineItem;

/* Using declarations: */
using namespace UISettingsDefs;

/** UIDetailsItem extension implementing set item. */
class UIDetailsSet : public UIDetailsItem
{
    Q_OBJECT;

public:

    /** RTTI item type. */
    enum { Type = UIDetailsItemType_Set };

    /** Constructs set item, passing pParent to the base-class. */
    UIDetailsSet(UIDetailsItem *pParent);
    /** Destructs set item. */
    virtual ~UIDetailsSet() /* override */;

    /** @name Item stuff.
      * @{ */
        /** Builds set based on passed @a pMachineItem.
          * @param  fFullSet  Brigns whether full set should be built.
          * @param  settings  Brings details related settings. */
        void buildSet(UIVirtualMachineItem *pMachineItem, bool fFullSet, const QMap<DetailsElementType, bool> &settings);

        /** Returns cached machine. */
        const CMachine &machine() const { return m_machine; }
        /** Returns whether set has cached details. */
        bool hasDetails() const { return m_fHasDetails; }
        /** Returns configuration access level. */
        ConfigurationAccessLevel configurationAccessLevel() const { return m_configurationAccessLevel; }
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
        virtual QString description() const /* override */;
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Adds child @a pItem. */
        virtual void addItem(UIDetailsItem *pItem) /* override */;
        /** Removes child @a pItem. */
        virtual void removeItem(UIDetailsItem *pItem) /* override */;

        /** Returns children items of certain @a enmType. */
        virtual QList<UIDetailsItem*> items(UIDetailsItemType type = UIDetailsItemType_Element) const /* override */;
        /** Returns whether there are children items of certain @a enmType. */
        virtual bool hasItems(UIDetailsItemType type = UIDetailsItemType_Element) const /* override */;
        /** Clears children items of certain @a enmType. */
        virtual void clearItems(UIDetailsItemType type = UIDetailsItemType_Element) /* override */;

        /** Returns details element of certain @a enmElementType. */
        UIDetailsElement *element(DetailsElementType enmElementType) const;
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

private slots:

    /** @name Event-handling stuff.
      * @{ */
        /** Handles machine-state change for item with @a strId. */
        void sltMachineStateChange(QString strId);
        /** Handles machine-attribute change for item with @a strId. */
        void sltMachineAttributesChange(QString strId);
    /** @} */

    /** @name Item stuff.
      * @{ */
        /** Updates item appearance. */
        void sltUpdateAppearance();
    /** @} */

private:

    /** Data field types. */
    enum SetItemData
    {
        /* Layout hints: */
        SetData_Margin,
        SetData_Spacing
    };

    /** @name Prepare/cleanup cascade.
      * @{ */
        /** Prepares set. */
        void prepareSet();
        /** Prepares connections. */
        void prepareConnections();
    /** @} */

    /** @name Item stuff.
      * @{ */
        /** Returns abstractly stored data value for certain @a iKey. */
        QVariant data(int iKey) const;

        /** Rebuilds set based on cached machine item. */
        void rebuildSet();

        /** Creates element of specified @a enmElementType in @a fOpen state. */
        UIDetailsElement *createElement(DetailsElementType enmElementType, bool fOpen);
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Enumerates item being placed into layout.
          * @param  inGroup   Will contain a list of items in preview group.
          * @param  outGroup  Will contain a list of items ouside of preview group.
          * @param  iAdditionalGroupHeight    Will have additional group height.
          * @param  iAdditionalPreviewHeight  Will have additional preview height. */
        void enumerateLayoutItems(QList<DetailsElementType> &inGroup,
                                  QList<DetailsElementType> &outGroup,
                                  int &iAdditionalGroupHeight,
                                  int &iAdditionalPreviewHeight) const;
    /** @} */

    /** @name Painting stuff.
      * @{ */
        /** Paints background using specified @a pPainter and certain @a pOptions. */
        void paintBackground(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions) const;
    /** @} */

    /** @name Item stuff.
      * @{ */
        /** Holds the machine-item this set is built for. */
        UIVirtualMachineItem           *m_pMachineItem;
        /** Holds whether whether full set should be built. */
        bool                            m_fFullSet;
        /** Holds the details related settings. */
        QMap<DetailsElementType, bool>  m_settings;

        /** Holds the machine reference. */
        CMachine                  m_machine;
        /** Holds whether set has details. */
        bool                      m_fHasDetails;
        /** Holds configuration access level. */
        ConfigurationAccessLevel  m_configurationAccessLevel;

        /** Holds the build step instance. */
        UIPrepareStep *m_pBuildStep;
        /** Holds the last step number. */
        int            m_iLastStepNumber;
        /** Holds the generated set ID. */
        QString        m_strSetId;
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Holds the map of generated detail elements. */
        QMap<int, UIDetailsItem*>  m_elements;
    /** @} */
};

#endif /* !___UIDetailsSet_h___ */
