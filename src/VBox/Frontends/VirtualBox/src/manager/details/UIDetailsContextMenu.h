/* $Id$ */
/** @file
 * VBox Qt GUI - UIDetailsContextMenu class declaration.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_manager_details_UIDetailsContextMenu_h
#define FEQT_INCLUDED_SRC_manager_details_UIDetailsContextMenu_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UIExtraDataDefs.h"

/* Forward declaration: */
class QListWidget;
class QListWidgetItem;
class UIDetailsModel;

/** QWidget subclass used as Details pane context menu. */
class UIDetailsContextMenu : public QIWithRetranslateUI2<QWidget>
{
    Q_OBJECT;

public:

    /** Context menu data fields. */
    enum DataField
    {
        DataField_Type = Qt::UserRole + 1,
        DataField_Name = Qt::UserRole + 2,
    };

    /** Constructs context-menu.
      * @param  pModel  Brings model object reference. */
    UIDetailsContextMenu(UIDetailsModel *pModel);

    /** Updates category check-states. */
    void updateCategoryStates();
    /** Updates option check-states for certain @a enmRequiredCategoryType. */
    void updateOptionStates(DetailsElementType enmRequiredCategoryType = DetailsElementType_Invalid);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Handles translation event for categories list. */
    void retranslateCategories();
    /** Handles translation event for options list. */
    void retranslateOptions();

private slots:

    /** Handles signal about category list-widget @a pItem hovered. */
    void sltCategoryItemEntered(QListWidgetItem *pItem);
    /** Handles signal about category list-widget @a pItem clicked. */
    void sltCategoryItemClicked(QListWidgetItem *pItem);
    /** Handles signal about current category list-widget @a pItem hovered. */
    void sltCategoryItemChanged(QListWidgetItem *pCurrent, QListWidgetItem *pPrevious);

    /** Handles signal about option list-widget @a pItem hovered. */
    void sltOptionItemEntered(QListWidgetItem *pItem);
    /** Handles signal about option list-widget @a pItem clicked. */
    void sltOptionItemClicked(QListWidgetItem *pItem);

private:

    /** Prepares all. */
    void prepare();

    /** (Re)populates categories. */
    void populateCategories();
    /** (Re)populates options. */
    void populateOptions();

    /** Adjusts both list widgets. */
    void adjustListWidgets();

    /** Creates category list item with specified @a icon. */
    QListWidgetItem *createCategoryItem(const QIcon &icon);
    /** Creates option list item. */
    QListWidgetItem *createOptionItem();

    /** Holds the model reference. */
    UIDetailsModel *m_pModel;

    /** Holds the categories list instance. */
    QListWidget *m_pListWidgetCategories;
    /** Holds the options list instance. */
    QListWidget *m_pListWidgetOptions;
};

#endif /* !FEQT_INCLUDED_SRC_manager_details_UIDetailsContextMenu_h */
