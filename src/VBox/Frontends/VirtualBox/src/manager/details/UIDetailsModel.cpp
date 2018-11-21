/* $Id$ */
/** @file
 * VBox Qt GUI - UIDetailsModel class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QAction>
# include <QGraphicsScene>
# include <QGraphicsSceneContextMenuEvent>
# include <QGraphicsView>
# include <QHBoxLayout>
# include <QListWidget>
# include <QMenu>
# include <QMetaEnum>

/* GUI includes: */
# include "UIConverter.h"
# include "UIDetails.h"
# include "UIDetailsModel.h"
# include "UIDetailsGroup.h"
# include "UIDetailsElement.h"
# include "UIExtraDataManager.h"
# include "UIIconPool.h"
# include "VBoxGlobal.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   Class UIDetailsContextMenu implementation.                                                                                   *
*********************************************************************************************************************************/

UIDetailsContextMenu::UIDetailsContextMenu(UIDetailsModel *pModel)
    : QIWithRetranslateUI2<QWidget>(0, Qt::Popup)
    , m_pModel(pModel)
    , m_pListWidgetCategories(0)
    , m_pListWidgetOptions(0)
{
    prepare();
}

void UIDetailsContextMenu::updateCategoryStates()
{
    /* Enumerate all the category items: */
    for (int i = 0; i < m_pListWidgetCategories->count(); ++i)
    {
        QListWidgetItem *pCategoryItem = m_pListWidgetCategories->item(i);
        if (pCategoryItem)
        {
            /* Apply check-state on per-enum basis: */
            const DetailsElementType enmCategoryType = pCategoryItem->data(DataField_Type).value<DetailsElementType>();
            pCategoryItem->setCheckState(m_pModel->categories().contains(enmCategoryType) ? Qt::Checked : Qt::Unchecked);
        }
    }
}

void UIDetailsContextMenu::updateOptionStates(DetailsElementType enmRequiredCategoryType /* = DetailsElementType_Invalid */)
{
    /* First make sure we really have category item selected: */
    QListWidgetItem *pCategoryItem = m_pListWidgetCategories->currentItem();
    if (!pCategoryItem)
        return;

    /* Then acquire category type and check if it is suitable: */
    const DetailsElementType enmCategoryType = pCategoryItem->data(DataField_Type).value<DetailsElementType>();
    if (enmRequiredCategoryType == DetailsElementType_Invalid)
        enmRequiredCategoryType = enmCategoryType;
    if (enmCategoryType != enmRequiredCategoryType)
        return;

    /* Handle known category types: */
    switch (enmRequiredCategoryType)
    {
        case DetailsElementType_General:
        {
            /* Enumerate all the option items: */
            for (int i = 0; i < m_pListWidgetOptions->count(); ++i)
            {
                QListWidgetItem *pOptionItem = m_pListWidgetOptions->item(i);
                if (pOptionItem)
                {
                    /* Apply check-state on per-enum basis: */
                    const UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral enmOptionType =
                        pOptionItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral>();
                    pOptionItem->setCheckState(m_pModel->optionsGeneral() & enmOptionType ? Qt::Checked : Qt::Unchecked);
                }
            }
            break;
        }
        case DetailsElementType_System:
        {
            /* Enumerate all the option items: */
            for (int i = 0; i < m_pListWidgetOptions->count(); ++i)
            {
                QListWidgetItem *pOptionItem = m_pListWidgetOptions->item(i);
                if (pOptionItem)
                {
                    /* Apply check-state on per-enum basis: */
                    const UIExtraDataMetaDefs::DetailsElementOptionTypeSystem enmOptionType =
                        pOptionItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeSystem>();
                    pOptionItem->setCheckState(m_pModel->optionsSystem() & enmOptionType ? Qt::Checked : Qt::Unchecked);
                }
            }
            break;
        }
        case DetailsElementType_Display:
        {
            /* Enumerate all the option items: */
            for (int i = 0; i < m_pListWidgetOptions->count(); ++i)
            {
                QListWidgetItem *pOptionItem = m_pListWidgetOptions->item(i);
                if (pOptionItem)
                {
                    /* Apply check-state on per-enum basis: */
                    const UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay enmOptionType =
                        pOptionItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay>();
                    pOptionItem->setCheckState(m_pModel->optionsDisplay() & enmOptionType ? Qt::Checked : Qt::Unchecked);
                }
            }
            break;
        }
        case DetailsElementType_Storage:
        {
            /* Enumerate all the option items: */
            for (int i = 0; i < m_pListWidgetOptions->count(); ++i)
            {
                QListWidgetItem *pOptionItem = m_pListWidgetOptions->item(i);
                if (pOptionItem)
                {
                    /* Apply check-state on per-enum basis: */
                    const UIExtraDataMetaDefs::DetailsElementOptionTypeStorage enmOptionType =
                        pOptionItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeStorage>();
                    pOptionItem->setCheckState(m_pModel->optionsStorage() & enmOptionType ? Qt::Checked : Qt::Unchecked);
                }
            }
            break;
        }
        case DetailsElementType_Audio:
        {
            /* Enumerate all the option items: */
            for (int i = 0; i < m_pListWidgetOptions->count(); ++i)
            {
                QListWidgetItem *pOptionItem = m_pListWidgetOptions->item(i);
                if (pOptionItem)
                {
                    /* Apply check-state on per-enum basis: */
                    const UIExtraDataMetaDefs::DetailsElementOptionTypeAudio enmOptionType =
                        pOptionItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeAudio>();
                    pOptionItem->setCheckState(m_pModel->optionsAudio() & enmOptionType ? Qt::Checked : Qt::Unchecked);
                }
            }
            break;
        }
        case DetailsElementType_Network:
        {
            /* Enumerate all the option items: */
            for (int i = 0; i < m_pListWidgetOptions->count(); ++i)
            {
                QListWidgetItem *pOptionItem = m_pListWidgetOptions->item(i);
                if (pOptionItem)
                {
                    /* Apply check-state on per-enum basis: */
                    const UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork enmOptionType =
                        pOptionItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork>();
                    pOptionItem->setCheckState(m_pModel->optionsNetwork() & enmOptionType ? Qt::Checked : Qt::Unchecked);
                }
            }
            break;
        }
        case DetailsElementType_Serial:
        {
            /* Enumerate all the option items: */
            for (int i = 0; i < m_pListWidgetOptions->count(); ++i)
            {
                QListWidgetItem *pOptionItem = m_pListWidgetOptions->item(i);
                if (pOptionItem)
                {
                    /* Apply check-state on per-enum basis: */
                    const UIExtraDataMetaDefs::DetailsElementOptionTypeSerial enmOptionType =
                        pOptionItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeSerial>();
                    pOptionItem->setCheckState(m_pModel->optionsSerial() & enmOptionType ? Qt::Checked : Qt::Unchecked);
                }
            }
            break;
        }
        case DetailsElementType_USB:
        {
            /* Enumerate all the option items: */
            for (int i = 0; i < m_pListWidgetOptions->count(); ++i)
            {
                QListWidgetItem *pOptionItem = m_pListWidgetOptions->item(i);
                if (pOptionItem)
                {
                    /* Apply check-state on per-enum basis: */
                    const UIExtraDataMetaDefs::DetailsElementOptionTypeUsb enmOptionType =
                        pOptionItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeUsb>();
                    pOptionItem->setCheckState(m_pModel->optionsUsb() & enmOptionType ? Qt::Checked : Qt::Unchecked);
                }
            }
            break;
        }
        case DetailsElementType_SF:
        {
            /* Enumerate all the option items: */
            for (int i = 0; i < m_pListWidgetOptions->count(); ++i)
            {
                QListWidgetItem *pOptionItem = m_pListWidgetOptions->item(i);
                if (pOptionItem)
                {
                    /* Apply check-state on per-enum basis: */
                    const UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders enmOptionType =
                        pOptionItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders>();
                    pOptionItem->setCheckState(m_pModel->optionsSharedFolders() & enmOptionType ? Qt::Checked : Qt::Unchecked);
                }
            }
            break;
        }
        case DetailsElementType_UI:
        {
            /* Enumerate all the option items: */
            for (int i = 0; i < m_pListWidgetOptions->count(); ++i)
            {
                QListWidgetItem *pOptionItem = m_pListWidgetOptions->item(i);
                if (pOptionItem)
                {
                    /* Apply check-state on per-enum basis: */
                    const UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface enmOptionType =
                        pOptionItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface>();
                    pOptionItem->setCheckState(m_pModel->optionsUserInterface() & enmOptionType ? Qt::Checked : Qt::Unchecked);
                }
            }
            break;
        }
        case DetailsElementType_Description:
        {
            /* Enumerate all the option items: */
            for (int i = 0; i < m_pListWidgetOptions->count(); ++i)
            {
                QListWidgetItem *pOptionItem = m_pListWidgetOptions->item(i);
                if (pOptionItem)
                {
                    /* Apply check-state on per-enum basis: */
                    const UIExtraDataMetaDefs::DetailsElementOptionTypeDescription enmOptionType =
                        pOptionItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeDescription>();
                    pOptionItem->setCheckState(m_pModel->optionsDescription() & enmOptionType ? Qt::Checked : Qt::Unchecked);
                }
            }
            break;
        }
        default:
            break;
    }
}

void UIDetailsContextMenu::retranslateUi()
{
    retranslateCategories();
    retranslateOptions();
    adjustListWidgets();
}

void UIDetailsContextMenu::retranslateCategories()
{
    /* Enumerate all the category items: */
    for (int i = 0; i < m_pListWidgetCategories->count(); ++i)
    {
        QListWidgetItem *pCategoryItem = m_pListWidgetCategories->item(i);
        if (pCategoryItem)
        {
            /* We can translate this thing on per-enum basis: */
            const DetailsElementType enmCategoryType = pCategoryItem->data(DataField_Type).value<DetailsElementType>();
            pCategoryItem->setText(gpConverter->toString(enmCategoryType));
        }
    }
}

void UIDetailsContextMenu::retranslateOptions()
{
    /* Acquire currently selected category item: */
    QListWidgetItem *pCategoryItem = m_pListWidgetCategories->currentItem();
    if (!pCategoryItem)
        return;

    /* Populate currently selected category options: */
    const DetailsElementType enmCategoryType = pCategoryItem->data(DataField_Type).value<DetailsElementType>();
    switch (enmCategoryType)
    {
        case DetailsElementType_General:
        {
            /* Enumerate all the option items: */
            for (int i = 0; i < m_pListWidgetOptions->count(); ++i)
            {
                QListWidgetItem *pOptionItem = m_pListWidgetOptions->item(i);
                if (pOptionItem)
                {
                    /* We can translate this thing on per-enum basis: */
                    const UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral enmOptionType =
                        pOptionItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral>();
                    pOptionItem->setText(gpConverter->toString(enmOptionType));
                }
            }
            break;
        }
        case DetailsElementType_System:
        {
            /* Enumerate all the option items: */
            for (int i = 0; i < m_pListWidgetOptions->count(); ++i)
            {
                QListWidgetItem *pOptionItem = m_pListWidgetOptions->item(i);
                if (pOptionItem)
                {
                    /* We can translate this thing on per-enum basis: */
                    const UIExtraDataMetaDefs::DetailsElementOptionTypeSystem enmOptionType =
                        pOptionItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeSystem>();
                    pOptionItem->setText(gpConverter->toString(enmOptionType));
                }
            }
            break;
        }
        case DetailsElementType_Display:
        {
            /* Enumerate all the option items: */
            for (int i = 0; i < m_pListWidgetOptions->count(); ++i)
            {
                QListWidgetItem *pOptionItem = m_pListWidgetOptions->item(i);
                if (pOptionItem)
                {
                    /* We can translate this thing on per-enum basis: */
                    const UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay enmOptionType =
                        pOptionItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay>();
                    pOptionItem->setText(gpConverter->toString(enmOptionType));
                }
            }
            break;
        }
        case DetailsElementType_Storage:
        {
            /* Enumerate all the option items: */
            for (int i = 0; i < m_pListWidgetOptions->count(); ++i)
            {
                QListWidgetItem *pOptionItem = m_pListWidgetOptions->item(i);
                if (pOptionItem)
                {
                    /* We can translate this thing on per-enum basis: */
                    const UIExtraDataMetaDefs::DetailsElementOptionTypeStorage enmOptionType =
                        pOptionItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeStorage>();
                    pOptionItem->setText(gpConverter->toString(enmOptionType));
                }
            }
            break;
        }
        case DetailsElementType_Audio:
        {
            /* Enumerate all the option items: */
            for (int i = 0; i < m_pListWidgetOptions->count(); ++i)
            {
                QListWidgetItem *pOptionItem = m_pListWidgetOptions->item(i);
                if (pOptionItem)
                {
                    /* We can translate this thing on per-enum basis: */
                    const UIExtraDataMetaDefs::DetailsElementOptionTypeAudio enmOptionType =
                        pOptionItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeAudio>();
                    pOptionItem->setText(gpConverter->toString(enmOptionType));
                }
            }
            break;
        }
        case DetailsElementType_Network:
        {
            /* Enumerate all the option items: */
            for (int i = 0; i < m_pListWidgetOptions->count(); ++i)
            {
                QListWidgetItem *pOptionItem = m_pListWidgetOptions->item(i);
                if (pOptionItem)
                {
                    /* We can translate this thing on per-enum basis: */
                    const UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork enmOptionType =
                        pOptionItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork>();
                    pOptionItem->setText(gpConverter->toString(enmOptionType));
                }
            }
            break;
        }
        case DetailsElementType_Serial:
        {
            /* Enumerate all the option items: */
            for (int i = 0; i < m_pListWidgetOptions->count(); ++i)
            {
                QListWidgetItem *pOptionItem = m_pListWidgetOptions->item(i);
                if (pOptionItem)
                {
                    /* We can translate this thing on per-enum basis: */
                    const UIExtraDataMetaDefs::DetailsElementOptionTypeSerial enmOptionType =
                        pOptionItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeSerial>();
                    pOptionItem->setText(gpConverter->toString(enmOptionType));
                }
            }
            break;
        }
        case DetailsElementType_USB:
        {
            /* Enumerate all the option items: */
            for (int i = 0; i < m_pListWidgetOptions->count(); ++i)
            {
                QListWidgetItem *pOptionItem = m_pListWidgetOptions->item(i);
                if (pOptionItem)
                {
                    /* We can translate this thing on per-enum basis: */
                    const UIExtraDataMetaDefs::DetailsElementOptionTypeUsb enmOptionType =
                        pOptionItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeUsb>();
                    pOptionItem->setText(gpConverter->toString(enmOptionType));
                }
            }
            break;
        }
        case DetailsElementType_SF:
        {
            /* Enumerate all the option items: */
            for (int i = 0; i < m_pListWidgetOptions->count(); ++i)
            {
                QListWidgetItem *pOptionItem = m_pListWidgetOptions->item(i);
                if (pOptionItem)
                {
                    /* We can translate this thing on per-enum basis: */
                    const UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders enmOptionType =
                        pOptionItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders>();
                    pOptionItem->setText(gpConverter->toString(enmOptionType));
                }
            }
            break;
        }
        case DetailsElementType_UI:
        {
            /* Enumerate all the option items: */
            for (int i = 0; i < m_pListWidgetOptions->count(); ++i)
            {
                QListWidgetItem *pOptionItem = m_pListWidgetOptions->item(i);
                if (pOptionItem)
                {
                    /* We can translate this thing on per-enum basis: */
                    const UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface enmOptionType =
                        pOptionItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface>();
                    pOptionItem->setText(gpConverter->toString(enmOptionType));
                }
            }
            break;
        }
        case DetailsElementType_Description:
        {
            /* Enumerate all the option items: */
            for (int i = 0; i < m_pListWidgetOptions->count(); ++i)
            {
                QListWidgetItem *pOptionItem = m_pListWidgetOptions->item(i);
                if (pOptionItem)
                {
                    /* We can translate this thing on per-enum basis: */
                    const UIExtraDataMetaDefs::DetailsElementOptionTypeDescription enmOptionType =
                        pOptionItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeDescription>();
                    pOptionItem->setText(gpConverter->toString(enmOptionType));
                }
            }
            break;
        }
        default:
            break;
    }
}

void UIDetailsContextMenu::sltCategoryItemEntered(QListWidgetItem *pItem)
{
    /* Choose hovered item as current one: */
    m_pListWidgetCategories->setCurrentItem(pItem);
}

void UIDetailsContextMenu::sltCategoryItemClicked(QListWidgetItem *pItem)
{
    /* Notify listeners: */
    const DetailsElementType enmCategoryType = pItem->data(DataField_Type).value<DetailsElementType>();

    /* Toggle element visibility status: */
    QMap<DetailsElementType, bool> categories = m_pModel->categories();
    if (categories.contains(enmCategoryType))
        categories.remove(enmCategoryType);
    else
        categories[enmCategoryType] = true;
    m_pModel->setCategories(categories);
}

void UIDetailsContextMenu::sltCategoryItemChanged(QListWidgetItem *, QListWidgetItem *)
{
    /* Update options list: */
    populateOptions();
    updateOptionStates();
    retranslateOptions();
}

void UIDetailsContextMenu::sltOptionItemEntered(QListWidgetItem *pItem)
{
    /* Choose hovered item as current one: */
    m_pListWidgetOptions->setCurrentItem(pItem);
}

void UIDetailsContextMenu::sltOptionItemClicked(QListWidgetItem *pItem)
{
    /* First make sure we really have category item selected: */
    QListWidgetItem *pCategoryItem = m_pListWidgetCategories->currentItem();
    if (!pCategoryItem)
        return;

    /* Then acquire category type: */
    const DetailsElementType enmCategoryType = pCategoryItem->data(DataField_Type).value<DetailsElementType>();

    /* Handle known category types: */
    switch (enmCategoryType)
    {
        case DetailsElementType_General:
        {
            /* Toggle element visibility status: */
            const UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral enmOptionType =
                pItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral>();
            m_pModel->setOptionsGeneral(static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral>(m_pModel->optionsGeneral() ^ enmOptionType));
            break;
        }
        case DetailsElementType_System:
        {
            /* Toggle element visibility status: */
            const UIExtraDataMetaDefs::DetailsElementOptionTypeSystem enmOptionType =
                pItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeSystem>();
            m_pModel->setOptionsSystem(static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeSystem>(m_pModel->optionsSystem() ^ enmOptionType));
            break;
        }
        case DetailsElementType_Display:
        {
            /* Toggle element visibility status: */
            const UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay enmOptionType =
                pItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay>();
            m_pModel->setOptionsDisplay(static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay>(m_pModel->optionsDisplay() ^ enmOptionType));
            break;
        }
        case DetailsElementType_Storage:
        {
            /* Toggle element visibility status: */
            const UIExtraDataMetaDefs::DetailsElementOptionTypeStorage enmOptionType =
                pItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeStorage>();
            m_pModel->setOptionsStorage(static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeStorage>(m_pModel->optionsStorage() ^ enmOptionType));
            break;
        }
        case DetailsElementType_Audio:
        {
            /* Toggle element visibility status: */
            const UIExtraDataMetaDefs::DetailsElementOptionTypeAudio enmOptionType =
                pItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeAudio>();
            m_pModel->setOptionsAudio(static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeAudio>(m_pModel->optionsAudio() ^ enmOptionType));
            break;
        }
        case DetailsElementType_Network:
        {
            /* Toggle element visibility status: */
            const UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork enmOptionType =
                pItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork>();
            m_pModel->setOptionsNetwork(static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork>(m_pModel->optionsNetwork() ^ enmOptionType));
            break;
        }
        case DetailsElementType_Serial:
        {
            /* Toggle element visibility status: */
            const UIExtraDataMetaDefs::DetailsElementOptionTypeSerial enmOptionType =
                pItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeSerial>();
            m_pModel->setOptionsSerial(static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeSerial>(m_pModel->optionsSerial() ^ enmOptionType));
            break;
        }
        case DetailsElementType_USB:
        {
            /* Toggle element visibility status: */
            const UIExtraDataMetaDefs::DetailsElementOptionTypeUsb enmOptionType =
                pItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeUsb>();
            m_pModel->setOptionsUsb(static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeUsb>(m_pModel->optionsUsb() ^ enmOptionType));
            break;
        }
        case DetailsElementType_SF:
        {
            /* Toggle element visibility status: */
            const UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders enmOptionType =
                pItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders>();
            m_pModel->setOptionsSharedFolders(static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders>(m_pModel->optionsSharedFolders() ^ enmOptionType));
            break;
        }
        case DetailsElementType_UI:
        {
            /* Toggle element visibility status: */
            const UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface enmOptionType =
                pItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface>();
            m_pModel->setOptionsUserInterface(static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface>(m_pModel->optionsUserInterface() ^ enmOptionType));
            break;
        }
        case DetailsElementType_Description:
        {
            /* Toggle element visibility status: */
            const UIExtraDataMetaDefs::DetailsElementOptionTypeDescription enmOptionType =
                pItem->data(DataField_Type).value<UIExtraDataMetaDefs::DetailsElementOptionTypeDescription>();
            m_pModel->setOptionsDescription(static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeDescription>(m_pModel->optionsDescription() ^ enmOptionType));
            break;
        }
        default:
            break;
    }
}

void UIDetailsContextMenu::prepare()
{
    /* Create main layout: */
    QHBoxLayout *pMainLayout = new QHBoxLayout(this);
    if (pMainLayout)
    {
        pMainLayout->setContentsMargins(0, 0, 0, 0);
        pMainLayout->setSpacing(1);

        /* Create list of categories: */
        m_pListWidgetCategories = new QListWidget(this);
        if (m_pListWidgetCategories)
        {
            m_pListWidgetCategories->setMouseTracking(true);
            m_pListWidgetCategories->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
            connect(m_pListWidgetCategories, &QListWidget::itemEntered,
                    this, &UIDetailsContextMenu::sltCategoryItemEntered);
            connect(m_pListWidgetCategories, &QListWidget::itemClicked,
                    this, &UIDetailsContextMenu::sltCategoryItemClicked);
            connect(m_pListWidgetCategories, &QListWidget::currentItemChanged,
                    this, &UIDetailsContextMenu::sltCategoryItemChanged);
            pMainLayout->addWidget(m_pListWidgetCategories);
        }

        /* Create list of options: */
        m_pListWidgetOptions = new QListWidget(this);
        if (m_pListWidgetOptions)
        {
            m_pListWidgetOptions->setMouseTracking(true);
            m_pListWidgetOptions->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
            connect(m_pListWidgetOptions, &QListWidget::itemEntered,
                    this, &UIDetailsContextMenu::sltOptionItemEntered);
            connect(m_pListWidgetOptions, &QListWidget::itemClicked,
                    this, &UIDetailsContextMenu::sltOptionItemClicked);
            pMainLayout->addWidget(m_pListWidgetOptions);
        }
    }

    /* Prepare lists: */
    populateCategories();
    populateOptions();
    /* Apply language settings: */
    retranslateUi();
}

void UIDetailsContextMenu::populateCategories()
{
    /* Clear category list initially: */
    m_pListWidgetCategories->clear();

    /* Enumerate all the known categories: */
    for (int i = DetailsElementType_Invalid + 1; i < DetailsElementType_Max; ++i)
    {
        /* Prepare current category type: */
        const DetailsElementType enmCategoryType = static_cast<DetailsElementType>(i);
        /* And create list-widget item of it: */
        QListWidgetItem *pCategoryItem = createCategoryItem(gpConverter->toIcon(enmCategoryType));
        if (pCategoryItem)
        {
            pCategoryItem->setData(DataField_Type, QVariant::fromValue(enmCategoryType));
            pCategoryItem->setCheckState(Qt::Unchecked);
        }
    }
}

void UIDetailsContextMenu::populateOptions()
{
    /* Clear option list initially: */
    m_pListWidgetOptions->clear();

    /* Acquire currently selected category item: */
    QListWidgetItem *pCategoryItem = m_pListWidgetCategories->currentItem();
    if (!pCategoryItem)
        return;

    /* We will use that one for all the options fetching: */
    const QMetaObject &smo = UIExtraDataMetaDefs::staticMetaObject;

    /* Populate currently selected category options: */
    const DetailsElementType enmCategoryType = pCategoryItem->data(DataField_Type).value<DetailsElementType>();
    switch (enmCategoryType)
    {
        case DetailsElementType_General:
        {
            /* Enumerate all the known options: */
            const int iEnumIndex = smo.indexOfEnumerator("DetailsElementOptionTypeGeneral");
            const QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
            for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
            {
                /* Prepare current option type: */
                const UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral enmOptionType =
                    static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
                /* Skip invalid and default types: */
                if (   enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral_Invalid
                    || enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral_Default)
                    continue;
                /* And create list-widget item of it: */
                QListWidgetItem *pOptionItem = createOptionItem();
                if (pOptionItem)
                {
                    pOptionItem->setData(DataField_Type, enmOptionType);
                    pOptionItem->setData(DataField_Name, gpConverter->toInternalString(enmOptionType));
                    pOptionItem->setCheckState(Qt::Unchecked);
                }
            }
            break;
        }
        case DetailsElementType_System:
        {
            /* Enumerate all the known options: */
            const int iEnumIndex = smo.indexOfEnumerator("DetailsElementOptionTypeSystem");
            const QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
            for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
            {
                /* Prepare current option type: */
                const UIExtraDataMetaDefs::DetailsElementOptionTypeSystem enmOptionType =
                    static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeSystem>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
                /* Skip invalid and default types: */
                if (   enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeSystem_Invalid
                    || enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeSystem_Default)
                    continue;
                /* And create list-widget item of it: */
                QListWidgetItem *pOptionItem = createOptionItem();
                if (pOptionItem)
                {
                    pOptionItem->setData(DataField_Type, QVariant::fromValue(enmOptionType));
                    pOptionItem->setData(DataField_Name, gpConverter->toInternalString(enmOptionType));
                    pOptionItem->setCheckState(Qt::Unchecked);
                }
            }
            break;
        }
        case DetailsElementType_Display:
        {
            /* Enumerate all the known options: */
            const int iEnumIndex = smo.indexOfEnumerator("DetailsElementOptionTypeDisplay");
            const QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
            for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
            {
                /* Prepare current option type: */
                const UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay enmOptionType =
                    static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
                /* Skip invalid and default types: */
                if (   enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay_Invalid
                    || enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay_Default)
                    continue;
                /* And create list-widget item of it: */
                QListWidgetItem *pOptionItem = createOptionItem();
                if (pOptionItem)
                {
                    pOptionItem->setData(DataField_Type, QVariant::fromValue(enmOptionType));
                    pOptionItem->setData(DataField_Name, gpConverter->toInternalString(enmOptionType));
                    pOptionItem->setCheckState(Qt::Unchecked);
                }
            }
            break;
        }
        case DetailsElementType_Storage:
        {
            /* Enumerate all the known options: */
            const int iEnumIndex = smo.indexOfEnumerator("DetailsElementOptionTypeStorage");
            const QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
            for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
            {
                /* Prepare current option type: */
                const UIExtraDataMetaDefs::DetailsElementOptionTypeStorage enmOptionType =
                    static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeStorage>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
                /* Skip invalid and default types: */
                if (   enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeStorage_Invalid
                    || enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeStorage_Default)
                    continue;
                /* And create list-widget item of it: */
                QListWidgetItem *pOptionItem = createOptionItem();
                if (pOptionItem)
                {
                    pOptionItem->setData(DataField_Type, QVariant::fromValue(enmOptionType));
                    pOptionItem->setData(DataField_Name, gpConverter->toInternalString(enmOptionType));
                    pOptionItem->setCheckState(Qt::Unchecked);
                }
            }
            break;
        }
        case DetailsElementType_Audio:
        {
            /* Enumerate all the known options: */
            const int iEnumIndex = smo.indexOfEnumerator("DetailsElementOptionTypeAudio");
            const QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
            for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
            {
                /* Prepare current option type: */
                const UIExtraDataMetaDefs::DetailsElementOptionTypeAudio enmOptionType =
                    static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeAudio>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
                /* Skip invalid and default types: */
                if (   enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeAudio_Invalid
                    || enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeAudio_Default)
                    continue;
                /* And create list-widget item of it: */
                QListWidgetItem *pOptionItem = createOptionItem();
                if (pOptionItem)
                {
                    pOptionItem->setData(DataField_Type, QVariant::fromValue(enmOptionType));
                    pOptionItem->setData(DataField_Name, gpConverter->toInternalString(enmOptionType));
                    pOptionItem->setCheckState(Qt::Unchecked);
                }
            }
            break;
        }
        case DetailsElementType_Network:
        {
            /* Enumerate all the known options: */
            const int iEnumIndex = smo.indexOfEnumerator("DetailsElementOptionTypeNetwork");
            const QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
            for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
            {
                /* Prepare current option type: */
                const UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork enmOptionType =
                    static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
                /* Skip invalid and default types: */
                if (   enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork_Invalid
                    || enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork_Default)
                    continue;
                /* And create list-widget item of it: */
                QListWidgetItem *pOptionItem = createOptionItem();
                if (pOptionItem)
                {
                    pOptionItem->setData(DataField_Type, QVariant::fromValue(enmOptionType));
                    pOptionItem->setData(DataField_Name, gpConverter->toInternalString(enmOptionType));
                    pOptionItem->setCheckState(Qt::Unchecked);
                }
            }
            break;
        }
        case DetailsElementType_Serial:
        {
            /* Enumerate all the known options: */
            const int iEnumIndex = smo.indexOfEnumerator("DetailsElementOptionTypeSerial");
            const QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
            for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
            {
                /* Prepare current option type: */
                const UIExtraDataMetaDefs::DetailsElementOptionTypeSerial enmOptionType =
                    static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeSerial>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
                /* Skip invalid and default types: */
                if (   enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeSerial_Invalid
                    || enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeSerial_Default)
                    continue;
                /* And create list-widget item of it: */
                QListWidgetItem *pOptionItem = createOptionItem();
                if (pOptionItem)
                {
                    pOptionItem->setData(DataField_Type, QVariant::fromValue(enmOptionType));
                    pOptionItem->setData(DataField_Name, gpConverter->toInternalString(enmOptionType));
                    pOptionItem->setCheckState(Qt::Unchecked);
                }
            }
            break;
        }
        case DetailsElementType_USB:
        {
            /* Enumerate all the known options: */
            const int iEnumIndex = smo.indexOfEnumerator("DetailsElementOptionTypeUsb");
            const QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
            for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
            {
                /* Prepare current option type: */
                const UIExtraDataMetaDefs::DetailsElementOptionTypeUsb enmOptionType =
                    static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeUsb>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
                /* Skip invalid and default types: */
                if (   enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeUsb_Invalid
                    || enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeUsb_Default)
                    continue;
                /* And create list-widget item of it: */
                QListWidgetItem *pOptionItem = createOptionItem();
                if (pOptionItem)
                {
                    pOptionItem->setData(DataField_Type, QVariant::fromValue(enmOptionType));
                    pOptionItem->setData(DataField_Name, gpConverter->toInternalString(enmOptionType));
                    pOptionItem->setCheckState(Qt::Unchecked);
                }
            }
            break;
        }
        case DetailsElementType_SF:
        {
            /* Enumerate all the known options: */
            const int iEnumIndex = smo.indexOfEnumerator("DetailsElementOptionTypeSharedFolders");
            const QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
            for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
            {
                /* Prepare current option type: */
                const UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders enmOptionType =
                    static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
                /* Skip invalid and default types: */
                if (   enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders_Invalid
                    || enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders_Default)
                    continue;
                /* And create list-widget item of it: */
                QListWidgetItem *pOptionItem = createOptionItem();
                if (pOptionItem)
                {
                    pOptionItem->setData(DataField_Type, QVariant::fromValue(enmOptionType));
                    pOptionItem->setData(DataField_Name, gpConverter->toInternalString(enmOptionType));
                    pOptionItem->setCheckState(Qt::Unchecked);
                }
            }
            break;
        }
        case DetailsElementType_UI:
        {
            /* Enumerate all the known options: */
            const int iEnumIndex = smo.indexOfEnumerator("DetailsElementOptionTypeUserInterface");
            const QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
            for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
            {
                /* Prepare current option type: */
                const UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface enmOptionType =
                    static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
                /* Skip invalid and default types: */
                if (   enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface_Invalid
                    || enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface_Default)
                    continue;
                /* And create list-widget item of it: */
                QListWidgetItem *pOptionItem = createOptionItem();
                if (pOptionItem)
                {
                    pOptionItem->setData(DataField_Type, QVariant::fromValue(enmOptionType));
                    pOptionItem->setData(DataField_Name, gpConverter->toInternalString(enmOptionType));
                    pOptionItem->setCheckState(Qt::Unchecked);
                }
            }
            break;
        }
        case DetailsElementType_Description:
        {
            /* Enumerate all the known options: */
            const int iEnumIndex = smo.indexOfEnumerator("DetailsElementOptionTypeDescription");
            const QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
            for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
            {
                /* Prepare current option type: */
                const UIExtraDataMetaDefs::DetailsElementOptionTypeDescription enmOptionType =
                    static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeDescription>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
                /* Skip invalid and default types: */
                if (   enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral_Invalid
                    || enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral_Default)
                    continue;
                /* And create list-widget item of it: */
                QListWidgetItem *pOptionItem = createOptionItem();
                if (pOptionItem)
                {
                    pOptionItem->setData(DataField_Type, QVariant::fromValue(enmOptionType));
                    pOptionItem->setData(DataField_Name, gpConverter->toInternalString(enmOptionType));
                    pOptionItem->setCheckState(Qt::Unchecked);
                }
            }
            break;
        }
        default:
            break;
    }
}

void UIDetailsContextMenu::adjustListWidgets()
{
    /* Include frame width: */
    int iW = 2 * m_pListWidgetCategories->frameWidth();
    int iH = iW;

    /* Include size hints: */
    iW += m_pListWidgetCategories->sizeHintForColumn(0);
    iH += m_pListWidgetCategories->sizeHintForRow(0) * m_pListWidgetCategories->count();

    /* Category list size is constant, options list size is vague: */
    m_pListWidgetCategories->setFixedSize(QSize(iW, iH));
    m_pListWidgetOptions->setFixedSize(QSize(iW * 1.5, iH));
}

QListWidgetItem *UIDetailsContextMenu::createCategoryItem(const QIcon &icon)
{
    QListWidgetItem *pItem = new QListWidgetItem(icon, QString(), m_pListWidgetCategories);
    if (pItem)
        m_pListWidgetCategories->addItem(pItem);
    return pItem;
}

QListWidgetItem *UIDetailsContextMenu::createOptionItem()
{
    QListWidgetItem *pItem = new QListWidgetItem(QString(), m_pListWidgetOptions);
    if (pItem)
        m_pListWidgetOptions->addItem(pItem);
    return pItem;
}


/*********************************************************************************************************************************
*   Class UIDetailsModel implementation.                                                                                         *
*********************************************************************************************************************************/

UIDetailsModel::UIDetailsModel(UIDetails *pParent)
    : QObject(pParent)
    , m_pDetails(pParent)
    , m_pScene(0)
    , m_pRoot(0)
    , m_pAnimationCallback(0)
    , m_pContextMenu(0)
{
    prepare();
}

UIDetailsModel::~UIDetailsModel()
{
    cleanup();
}

QGraphicsScene *UIDetailsModel::scene() const
{
    return m_pScene;
}

QGraphicsView *UIDetailsModel::paintDevice() const
{
    if (m_pScene && !m_pScene->views().isEmpty())
        return m_pScene->views().first();
    return 0;
}

QGraphicsItem *UIDetailsModel::itemAt(const QPointF &position) const
{
    return scene()->itemAt(position, QTransform());
}

UIDetailsItem *UIDetailsModel::root() const
{
    return m_pRoot;
}

void UIDetailsModel::updateLayout()
{
    /* Prepare variables: */
    const QSize viewportSize = paintDevice()->viewport()->size();
    const QSize rootSize = viewportSize.expandedTo(m_pRoot->minimumSizeHint().toSize());

    /* Move root: */
    m_pRoot->setPos(0, 0);
    /* Resize root: */
    m_pRoot->resize(rootSize);
    /* Layout root content: */
    m_pRoot->updateLayout();
}

void UIDetailsModel::setItems(const QList<UIVirtualMachineItem*> &items)
{
    m_pRoot->buildGroup(items);
}

void UIDetailsModel::setCategories(const QMap<DetailsElementType, bool> &categories)
{
    m_categories = categories;
    m_pRoot->rebuildGroup();
    m_pContextMenu->updateCategoryStates();
}

void UIDetailsModel::setOptionsGeneral(UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral fOptionsGeneral)
{
    m_fOptionsGeneral = fOptionsGeneral;
    m_pRoot->rebuildGroup();
    m_pContextMenu->updateOptionStates(DetailsElementType_General);
}

void UIDetailsModel::setOptionsSystem(UIExtraDataMetaDefs::DetailsElementOptionTypeSystem fOptionsSystem)
{
    m_fOptionsSystem = fOptionsSystem;
    m_pRoot->rebuildGroup();
    m_pContextMenu->updateOptionStates(DetailsElementType_System);
}

void UIDetailsModel::setOptionsDisplay(UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay fOptionsDisplay)
{
    m_fOptionsDisplay = fOptionsDisplay;
    m_pRoot->rebuildGroup();
    m_pContextMenu->updateOptionStates(DetailsElementType_Display);
}

void UIDetailsModel::setOptionsStorage(UIExtraDataMetaDefs::DetailsElementOptionTypeStorage fOptionsStorage)
{
    m_fOptionsStorage = fOptionsStorage;
    m_pRoot->rebuildGroup();
    m_pContextMenu->updateOptionStates(DetailsElementType_Storage);
}

void UIDetailsModel::setOptionsAudio(UIExtraDataMetaDefs::DetailsElementOptionTypeAudio fOptionsAudio)
{
    m_fOptionsAudio = fOptionsAudio;
    m_pRoot->rebuildGroup();
    m_pContextMenu->updateOptionStates(DetailsElementType_Audio);
}

void UIDetailsModel::setOptionsNetwork(UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork fOptionsNetwork)
{
    m_fOptionsNetwork = fOptionsNetwork;
    m_pRoot->rebuildGroup();
    m_pContextMenu->updateOptionStates(DetailsElementType_Network);
}

void UIDetailsModel::setOptionsSerial(UIExtraDataMetaDefs::DetailsElementOptionTypeSerial fOptionsSerial)
{
    m_fOptionsSerial = fOptionsSerial;
    m_pRoot->rebuildGroup();
    m_pContextMenu->updateOptionStates(DetailsElementType_Serial);
}

void UIDetailsModel::setOptionsUsb(UIExtraDataMetaDefs::DetailsElementOptionTypeUsb fOptionsUsb)
{
    m_fOptionsUsb = fOptionsUsb;
    m_pRoot->rebuildGroup();
    m_pContextMenu->updateOptionStates(DetailsElementType_USB);
}

void UIDetailsModel::setOptionsSharedFolders(UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders fOptionsSharedFolders)
{
    m_fOptionsSharedFolders = fOptionsSharedFolders;
    m_pRoot->rebuildGroup();
    m_pContextMenu->updateOptionStates(DetailsElementType_SF);
}

void UIDetailsModel::setOptionsUserInterface(UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface fOptionsUserInterface)
{
    m_fOptionsUserInterface = fOptionsUserInterface;
    m_pRoot->rebuildGroup();
    m_pContextMenu->updateOptionStates(DetailsElementType_UI);
}

void UIDetailsModel::setOptionsDescription(UIExtraDataMetaDefs::DetailsElementOptionTypeDescription fOptionsDescription)
{
    m_fOptionsDescription = fOptionsDescription;
    m_pRoot->rebuildGroup();
    m_pContextMenu->updateOptionStates(DetailsElementType_Description);
}

void UIDetailsModel::sltHandleViewResize()
{
    updateLayout();
}

void UIDetailsModel::sltHandleSlidingStarted()
{
    m_pRoot->stopBuildingGroup();
}

void UIDetailsModel::sltHandleToggleStarted()
{
    m_pRoot->stopBuildingGroup();
}

void UIDetailsModel::sltHandleToggleFinished()
{
    m_pRoot->rebuildGroup();
}

void UIDetailsModel::sltHandleExtraDataCategoriesChange()
{
    loadDetailsCategories();
    m_pContextMenu->updateCategoryStates();
    m_pRoot->rebuildGroup();
}

void UIDetailsModel::sltHandleExtraDataOptionsChange(DetailsElementType enmType)
{
    loadDetailsOptions(enmType);
    m_pContextMenu->updateOptionStates(enmType);
    m_pRoot->rebuildGroup();
}

bool UIDetailsModel::eventFilter(QObject *pObject, QEvent *pEvent)
{
    /* Handle allowed context-menu events: */
    if (pObject == scene() && pEvent->type() == QEvent::GraphicsSceneContextMenu)
        return processContextMenuEvent(static_cast<QGraphicsSceneContextMenuEvent*>(pEvent));

    /* Call to base-class: */
    return QObject::eventFilter(pObject, pEvent);
}

void UIDetailsModel::sltToggleElements(DetailsElementType type, bool fToggled)
{
    /* Make sure it is not started yet: */
    if (m_pAnimationCallback)
        return;

    /* Prepare/configure animation callback: */
    m_pAnimationCallback = new UIDetailsElementAnimationCallback(this, type, fToggled);
    connect(m_pAnimationCallback, SIGNAL(sigAllAnimationFinished(DetailsElementType, bool)),
            this, SLOT(sltToggleAnimationFinished(DetailsElementType, bool)), Qt::QueuedConnection);
    /* For each the set of the group: */
    foreach (UIDetailsItem *pSetItem, m_pRoot->items())
    {
        /* For each the element of the set: */
        foreach (UIDetailsItem *pElementItem, pSetItem->items())
        {
            /* Get each element: */
            UIDetailsElement *pElement = pElementItem->toElement();
            /* Check if this element is of required type: */
            if (pElement->elementType() == type)
            {
                if (fToggled && pElement->isClosed())
                {
                    m_pAnimationCallback->addNotifier(pElement);
                    pElement->open();
                }
                else if (!fToggled && pElement->isOpened())
                {
                    m_pAnimationCallback->addNotifier(pElement);
                    pElement->close();
                }
            }
        }
    }
    /* Update layout: */
    updateLayout();
}

void UIDetailsModel::sltToggleAnimationFinished(DetailsElementType enmType, bool fToggled)
{
    /* Cleanup animation callback: */
    delete m_pAnimationCallback;
    m_pAnimationCallback = 0;

    /* Mark animation finished: */
    foreach (UIDetailsItem *pSetItem, m_pRoot->items())
    {
        foreach (UIDetailsItem *pElementItem, pSetItem->items())
        {
            UIDetailsElement *pElement = pElementItem->toElement();
            if (pElement->elementType() == enmType)
                pElement->markAnimationFinished();
        }
    }
    /* Update layout: */
    updateLayout();

    /* Update element open/close status: */
    if (m_categories.contains(enmType))
        m_categories[enmType] = fToggled;
}

void UIDetailsModel::prepare()
{
    /* Prepare things: */
    prepareScene();
    prepareRoot();
    prepareContextMenu();
    loadSettings();
}

void UIDetailsModel::prepareScene()
{
    m_pScene = new QGraphicsScene(this);
    if (m_pScene)
        m_pScene->installEventFilter(this);
}

void UIDetailsModel::prepareRoot()
{
    m_pRoot = new UIDetailsGroup(scene());
}

void UIDetailsModel::prepareContextMenu()
{
    m_pContextMenu = new UIDetailsContextMenu(this);
}

void UIDetailsModel::loadSettings()
{
    loadDetailsCategories();
    loadDetailsOptions();
}

void UIDetailsModel::loadDetailsCategories()
{
    m_categories = gEDataManager->selectorWindowDetailsElements();
    m_pContextMenu->updateCategoryStates();
}

void UIDetailsModel::loadDetailsOptions(DetailsElementType enmType /* = DetailsElementType_Invalid */)
{
    /* We will handle DetailsElementType_Invalid as a request to load everything. */

    if (enmType == DetailsElementType_General || enmType == DetailsElementType_Invalid)
    {
        m_fOptionsGeneral = UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral_Invalid;
        foreach (const QString &strOption, gEDataManager->vboxManagerDetailsPaneElementOptions(DetailsElementType_General))
        {
            const UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral enmOption =
                gpConverter->fromInternalString<UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral>(strOption);
            if (enmOption != UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral_Invalid)
                m_fOptionsGeneral = static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral>(m_fOptionsGeneral | enmOption);
        }
        if (m_fOptionsGeneral == UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral_Invalid)
            m_fOptionsGeneral = UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral_Default;
    }

    if (enmType == DetailsElementType_System || enmType == DetailsElementType_Invalid)
    {
        m_fOptionsSystem = UIExtraDataMetaDefs::DetailsElementOptionTypeSystem_Invalid;
        foreach (const QString &strOption, gEDataManager->vboxManagerDetailsPaneElementOptions(DetailsElementType_System))
        {
            const UIExtraDataMetaDefs::DetailsElementOptionTypeSystem enmOption =
                gpConverter->fromInternalString<UIExtraDataMetaDefs::DetailsElementOptionTypeSystem>(strOption);
            if (enmOption != UIExtraDataMetaDefs::DetailsElementOptionTypeSystem_Invalid)
                m_fOptionsSystem = static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeSystem>(m_fOptionsSystem | enmOption);
        }
        if (m_fOptionsSystem == UIExtraDataMetaDefs::DetailsElementOptionTypeSystem_Invalid)
            m_fOptionsSystem = UIExtraDataMetaDefs::DetailsElementOptionTypeSystem_Default;
    }

    if (enmType == DetailsElementType_Display || enmType == DetailsElementType_Invalid)
    {
        m_fOptionsDisplay = UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay_Invalid;
        foreach (const QString &strOption, gEDataManager->vboxManagerDetailsPaneElementOptions(DetailsElementType_Display))
        {
            const UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay enmOption =
                gpConverter->fromInternalString<UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay>(strOption);
            if (enmOption != UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay_Invalid)
                m_fOptionsDisplay = static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay>(m_fOptionsDisplay | enmOption);
        }
        if (m_fOptionsDisplay == UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay_Invalid)
            m_fOptionsDisplay = UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay_Default;
    }

    if (enmType == DetailsElementType_Storage || enmType == DetailsElementType_Invalid)
    {
        m_fOptionsStorage = UIExtraDataMetaDefs::DetailsElementOptionTypeStorage_Invalid;
        foreach (const QString &strOption, gEDataManager->vboxManagerDetailsPaneElementOptions(DetailsElementType_Storage))
        {
            const UIExtraDataMetaDefs::DetailsElementOptionTypeStorage enmOption =
                gpConverter->fromInternalString<UIExtraDataMetaDefs::DetailsElementOptionTypeStorage>(strOption);
            if (enmOption != UIExtraDataMetaDefs::DetailsElementOptionTypeStorage_Invalid)
                m_fOptionsStorage = static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeStorage>(m_fOptionsStorage | enmOption);
        }
        if (m_fOptionsStorage == UIExtraDataMetaDefs::DetailsElementOptionTypeStorage_Invalid)
            m_fOptionsStorage = UIExtraDataMetaDefs::DetailsElementOptionTypeStorage_Default;
    }

    if (enmType == DetailsElementType_Audio || enmType == DetailsElementType_Invalid)
    {
        m_fOptionsAudio = UIExtraDataMetaDefs::DetailsElementOptionTypeAudio_Invalid;
        foreach (const QString &strOption, gEDataManager->vboxManagerDetailsPaneElementOptions(DetailsElementType_Audio))
        {
            const UIExtraDataMetaDefs::DetailsElementOptionTypeAudio enmOption =
                gpConverter->fromInternalString<UIExtraDataMetaDefs::DetailsElementOptionTypeAudio>(strOption);
            if (enmOption != UIExtraDataMetaDefs::DetailsElementOptionTypeAudio_Invalid)
                m_fOptionsAudio = static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeAudio>(m_fOptionsAudio | enmOption);
        }
        if (m_fOptionsAudio == UIExtraDataMetaDefs::DetailsElementOptionTypeAudio_Invalid)
            m_fOptionsAudio = UIExtraDataMetaDefs::DetailsElementOptionTypeAudio_Default;
    }

    if (enmType == DetailsElementType_Network || enmType == DetailsElementType_Invalid)
    {
        m_fOptionsNetwork = UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork_Invalid;
        foreach (const QString &strOption, gEDataManager->vboxManagerDetailsPaneElementOptions(DetailsElementType_Network))
        {
            const UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork enmOption =
                gpConverter->fromInternalString<UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork>(strOption);
            if (enmOption != UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork_Invalid)
                m_fOptionsNetwork = static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork>(m_fOptionsNetwork | enmOption);
        }
        if (m_fOptionsNetwork == UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork_Invalid)
            m_fOptionsNetwork = UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork_Default;
    }

    if (enmType == DetailsElementType_Serial || enmType == DetailsElementType_Invalid)
    {
        m_fOptionsSerial = UIExtraDataMetaDefs::DetailsElementOptionTypeSerial_Invalid;
        foreach (const QString &strOption, gEDataManager->vboxManagerDetailsPaneElementOptions(DetailsElementType_Serial))
        {
            const UIExtraDataMetaDefs::DetailsElementOptionTypeSerial enmOption =
                gpConverter->fromInternalString<UIExtraDataMetaDefs::DetailsElementOptionTypeSerial>(strOption);
            if (enmOption != UIExtraDataMetaDefs::DetailsElementOptionTypeSerial_Invalid)
                m_fOptionsSerial = static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeSerial>(m_fOptionsSerial | enmOption);
        }
        if (m_fOptionsSerial == UIExtraDataMetaDefs::DetailsElementOptionTypeSerial_Invalid)
            m_fOptionsSerial = UIExtraDataMetaDefs::DetailsElementOptionTypeSerial_Default;
    }

    if (enmType == DetailsElementType_USB || enmType == DetailsElementType_Invalid)
    {
        m_fOptionsUsb = UIExtraDataMetaDefs::DetailsElementOptionTypeUsb_Invalid;
        foreach (const QString &strOption, gEDataManager->vboxManagerDetailsPaneElementOptions(DetailsElementType_USB))
        {
            const UIExtraDataMetaDefs::DetailsElementOptionTypeUsb enmOption =
                gpConverter->fromInternalString<UIExtraDataMetaDefs::DetailsElementOptionTypeUsb>(strOption);
            if (enmOption != UIExtraDataMetaDefs::DetailsElementOptionTypeUsb_Invalid)
                m_fOptionsUsb = static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeUsb>(m_fOptionsUsb | enmOption);
        }
        if (m_fOptionsUsb == UIExtraDataMetaDefs::DetailsElementOptionTypeUsb_Invalid)
            m_fOptionsUsb = UIExtraDataMetaDefs::DetailsElementOptionTypeUsb_Default;
    }

    if (enmType == DetailsElementType_SF || enmType == DetailsElementType_Invalid)
    {
        m_fOptionsSharedFolders = UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders_Invalid;
        foreach (const QString &strOption, gEDataManager->vboxManagerDetailsPaneElementOptions(DetailsElementType_SF))
        {
            const UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders enmOption =
                gpConverter->fromInternalString<UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders>(strOption);
            if (enmOption != UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders_Invalid)
                m_fOptionsSharedFolders = static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders>(m_fOptionsSharedFolders | enmOption);
        }
        if (m_fOptionsSharedFolders == UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders_Invalid)
            m_fOptionsSharedFolders = UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders_Default;
    }

    if (enmType == DetailsElementType_UI || enmType == DetailsElementType_Invalid)
    {
        m_fOptionsUserInterface = UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface_Invalid;
        foreach (const QString &strOption, gEDataManager->vboxManagerDetailsPaneElementOptions(DetailsElementType_UI))
        {
            const UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface enmOption =
                gpConverter->fromInternalString<UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface>(strOption);
            if (enmOption != UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface_Invalid)
                m_fOptionsUserInterface = static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface>(m_fOptionsUserInterface | enmOption);
        }
        if (m_fOptionsUserInterface == UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface_Invalid)
            m_fOptionsUserInterface = UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface_Default;
    }

    if (enmType == DetailsElementType_Description || enmType == DetailsElementType_Invalid)
    {
        m_fOptionsDescription = UIExtraDataMetaDefs::DetailsElementOptionTypeDescription_Invalid;
        foreach (const QString &strOption, gEDataManager->vboxManagerDetailsPaneElementOptions(DetailsElementType_Description))
        {
            const UIExtraDataMetaDefs::DetailsElementOptionTypeDescription enmOption =
                gpConverter->fromInternalString<UIExtraDataMetaDefs::DetailsElementOptionTypeDescription>(strOption);
            if (enmOption != UIExtraDataMetaDefs::DetailsElementOptionTypeDescription_Invalid)
                m_fOptionsDescription = static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeDescription>(m_fOptionsDescription | enmOption);
        }
        if (m_fOptionsDescription == UIExtraDataMetaDefs::DetailsElementOptionTypeDescription_Invalid)
            m_fOptionsDescription = UIExtraDataMetaDefs::DetailsElementOptionTypeDescription_Default;
    }

    m_pContextMenu->updateOptionStates();
}

void UIDetailsModel::saveDetailsOptions()
{
    /* We will use that one for all the options fetching: */
    const QMetaObject &smo = UIExtraDataMetaDefs::staticMetaObject;
    int iEnumIndex = -1;

    /* General options: */
    iEnumIndex = smo.indexOfEnumerator("DetailsElementOptionTypeGeneral");
    if (iEnumIndex != -1)
    {
        bool fDefault = true;
        QStringList options;
        const QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
        for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
        {
            /* Prepare current option type: */
            const UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral enmOptionType =
                static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
            /* Skip invalid and default types: */
            if (   enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral_Invalid
                || enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral_Default)
                continue;
            /* If option type enabled: */
            if (m_fOptionsGeneral & enmOptionType)
            {
                /* Add it to the list: */
                options << gpConverter->toInternalString(enmOptionType);
                /* Make sure item is included by default: */
                if (!(UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral_Default & enmOptionType))
                    fDefault = false;
            }
            /* If option type disabled: */
            else
            {
                /* Make sure item is excluded by default: */
                if (UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral_Default & enmOptionType)
                    fDefault = false;
            }
            /* Save options: */
            if (!fDefault)
                gEDataManager->setVBoxManagerDetailsPaneElementOptions(DetailsElementType_General, options);
            else
                gEDataManager->setVBoxManagerDetailsPaneElementOptions(DetailsElementType_General, QStringList());
        }
    }

    /* System options: */
    iEnumIndex = smo.indexOfEnumerator("DetailsElementOptionTypeSystem");
    if (iEnumIndex != -1)
    {
        bool fDefault = true;
        QStringList options;
        const QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
        for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
        {
            /* Prepare current option type: */
            const UIExtraDataMetaDefs::DetailsElementOptionTypeSystem enmOptionType =
                static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeSystem>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
            /* Skip invalid and default types: */
            if (   enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeSystem_Invalid
                || enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeSystem_Default)
                continue;
            /* If option type enabled: */
            if (m_fOptionsSystem & enmOptionType)
            {
                /* Add it to the list: */
                options << gpConverter->toInternalString(enmOptionType);
                /* Make sure item is included by default: */
                if (!(UIExtraDataMetaDefs::DetailsElementOptionTypeSystem_Default & enmOptionType))
                    fDefault = false;
            }
            /* If option type disabled: */
            else
            {
                /* Make sure item is excluded by default: */
                if (UIExtraDataMetaDefs::DetailsElementOptionTypeSystem_Default & enmOptionType)
                    fDefault = false;
            }
            /* Save options: */
            if (!fDefault)
                gEDataManager->setVBoxManagerDetailsPaneElementOptions(DetailsElementType_System, options);
            else
                gEDataManager->setVBoxManagerDetailsPaneElementOptions(DetailsElementType_System, QStringList());
        }
    }

    /* Display options: */
    iEnumIndex = smo.indexOfEnumerator("DetailsElementOptionTypeDisplay");
    if (iEnumIndex != -1)
    {
        bool fDefault = true;
        QStringList options;
        const QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
        for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
        {
            /* Prepare current option type: */
            const UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay enmOptionType =
                static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
            /* Skip invalid and default types: */
            if (   enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay_Invalid
                || enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay_Default)
                continue;
            /* If option type enabled: */
            if (m_fOptionsDisplay & enmOptionType)
            {
                /* Add it to the list: */
                options << gpConverter->toInternalString(enmOptionType);
                /* Make sure item is included by default: */
                if (!(UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay_Default & enmOptionType))
                    fDefault = false;
            }
            /* If option type disabled: */
            else
            {
                /* Make sure item is excluded by default: */
                if (UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay_Default & enmOptionType)
                    fDefault = false;
            }
            /* Save options: */
            if (!fDefault)
                gEDataManager->setVBoxManagerDetailsPaneElementOptions(DetailsElementType_Display, options);
            else
                gEDataManager->setVBoxManagerDetailsPaneElementOptions(DetailsElementType_Display, QStringList());
        }
    }

    /* Storage options: */
    iEnumIndex = smo.indexOfEnumerator("DetailsElementOptionTypeStorage");
    if (iEnumIndex != -1)
    {
        bool fDefault = true;
        QStringList options;
        const QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
        for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
        {
            /* Prepare current option type: */
            const UIExtraDataMetaDefs::DetailsElementOptionTypeStorage enmOptionType =
                static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeStorage>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
            /* Skip invalid and default types: */
            if (   enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeStorage_Invalid
                || enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeStorage_Default)
                continue;
            /* If option type enabled: */
            if (m_fOptionsStorage & enmOptionType)
            {
                /* Add it to the list: */
                options << gpConverter->toInternalString(enmOptionType);
                /* Make sure item is included by default: */
                if (!(UIExtraDataMetaDefs::DetailsElementOptionTypeStorage_Default & enmOptionType))
                    fDefault = false;
            }
            /* If option type disabled: */
            else
            {
                /* Make sure item is excluded by default: */
                if (UIExtraDataMetaDefs::DetailsElementOptionTypeStorage_Default & enmOptionType)
                    fDefault = false;
            }
            /* Save options: */
            if (!fDefault)
                gEDataManager->setVBoxManagerDetailsPaneElementOptions(DetailsElementType_Storage, options);
            else
                gEDataManager->setVBoxManagerDetailsPaneElementOptions(DetailsElementType_Storage, QStringList());
        }
    }

    /* Audio options: */
    iEnumIndex = smo.indexOfEnumerator("DetailsElementOptionTypeAudio");
    if (iEnumIndex != -1)
    {
        bool fDefault = true;
        QStringList options;
        const QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
        for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
        {
            /* Prepare current option type: */
            const UIExtraDataMetaDefs::DetailsElementOptionTypeAudio enmOptionType =
                static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeAudio>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
            /* Skip invalid and default types: */
            if (   enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeAudio_Invalid
                || enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeAudio_Default)
                continue;
            /* If option type enabled: */
            if (m_fOptionsAudio & enmOptionType)
            {
                /* Add it to the list: */
                options << gpConverter->toInternalString(enmOptionType);
                /* Make sure item is included by default: */
                if (!(UIExtraDataMetaDefs::DetailsElementOptionTypeAudio_Default & enmOptionType))
                    fDefault = false;
            }
            /* If option type disabled: */
            else
            {
                /* Make sure item is excluded by default: */
                if (UIExtraDataMetaDefs::DetailsElementOptionTypeAudio_Default & enmOptionType)
                    fDefault = false;
            }
            /* Save options: */
            if (!fDefault)
                gEDataManager->setVBoxManagerDetailsPaneElementOptions(DetailsElementType_Audio, options);
            else
                gEDataManager->setVBoxManagerDetailsPaneElementOptions(DetailsElementType_Audio, QStringList());
        }
    }

    /* Network options: */
    iEnumIndex = smo.indexOfEnumerator("DetailsElementOptionTypeNetwork");
    if (iEnumIndex != -1)
    {
        bool fDefault = true;
        QStringList options;
        const QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
        for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
        {
            /* Prepare current option type: */
            const UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork enmOptionType =
                static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
            /* Skip invalid and default types: */
            if (   enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork_Invalid
                || enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork_Default)
                continue;
            /* If option type enabled: */
            if (m_fOptionsNetwork & enmOptionType)
            {
                /* Add it to the list: */
                options << gpConverter->toInternalString(enmOptionType);
                /* Make sure item is included by default: */
                if (!(UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork_Default & enmOptionType))
                    fDefault = false;
            }
            /* If option type disabled: */
            else
            {
                /* Make sure item is excluded by default: */
                if (UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork_Default & enmOptionType)
                    fDefault = false;
            }
            /* Save options: */
            if (!fDefault)
                gEDataManager->setVBoxManagerDetailsPaneElementOptions(DetailsElementType_Network, options);
            else
                gEDataManager->setVBoxManagerDetailsPaneElementOptions(DetailsElementType_Network, QStringList());
        }
    }

    /* Serial options: */
    iEnumIndex = smo.indexOfEnumerator("DetailsElementOptionTypeSerial");
    if (iEnumIndex != -1)
    {
        bool fDefault = true;
        QStringList options;
        const QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
        for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
        {
            /* Prepare current option type: */
            const UIExtraDataMetaDefs::DetailsElementOptionTypeSerial enmOptionType =
                static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeSerial>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
            /* Skip invalid and default types: */
            if (   enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeSerial_Invalid
                || enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeSerial_Default)
                continue;
            /* If option type enabled: */
            if (m_fOptionsSerial & enmOptionType)
            {
                /* Add it to the list: */
                options << gpConverter->toInternalString(enmOptionType);
                /* Make sure item is included by default: */
                if (!(UIExtraDataMetaDefs::DetailsElementOptionTypeSerial_Default & enmOptionType))
                    fDefault = false;
            }
            /* If option type disabled: */
            else
            {
                /* Make sure item is excluded by default: */
                if (UIExtraDataMetaDefs::DetailsElementOptionTypeSerial_Default & enmOptionType)
                    fDefault = false;
            }
            /* Save options: */
            if (!fDefault)
                gEDataManager->setVBoxManagerDetailsPaneElementOptions(DetailsElementType_Serial, options);
            else
                gEDataManager->setVBoxManagerDetailsPaneElementOptions(DetailsElementType_Serial, QStringList());
        }
    }

    /* Usb options: */
    iEnumIndex = smo.indexOfEnumerator("DetailsElementOptionTypeUsb");
    if (iEnumIndex != -1)
    {
        bool fDefault = true;
        QStringList options;
        const QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
        for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
        {
            /* Prepare current option type: */
            const UIExtraDataMetaDefs::DetailsElementOptionTypeUsb enmOptionType =
                static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeUsb>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
            /* Skip invalid and default types: */
            if (   enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeUsb_Invalid
                || enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeUsb_Default)
                continue;
            /* If option type enabled: */
            if (m_fOptionsUsb & enmOptionType)
            {
                /* Add it to the list: */
                options << gpConverter->toInternalString(enmOptionType);
                /* Make sure item is included by default: */
                if (!(UIExtraDataMetaDefs::DetailsElementOptionTypeUsb_Default & enmOptionType))
                    fDefault = false;
            }
            /* If option type disabled: */
            else
            {
                /* Make sure item is excluded by default: */
                if (UIExtraDataMetaDefs::DetailsElementOptionTypeUsb_Default & enmOptionType)
                    fDefault = false;
            }
            /* Save options: */
            if (!fDefault)
                gEDataManager->setVBoxManagerDetailsPaneElementOptions(DetailsElementType_USB, options);
            else
                gEDataManager->setVBoxManagerDetailsPaneElementOptions(DetailsElementType_USB, QStringList());
        }
    }

    /* SharedFolders options: */
    iEnumIndex = smo.indexOfEnumerator("DetailsElementOptionTypeSharedFolders");
    if (iEnumIndex != -1)
    {
        bool fDefault = true;
        QStringList options;
        const QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
        for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
        {
            /* Prepare current option type: */
            const UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders enmOptionType =
                static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
            /* Skip invalid and default types: */
            if (   enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders_Invalid
                || enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders_Default)
                continue;
            /* If option type enabled: */
            if (m_fOptionsSharedFolders & enmOptionType)
            {
                /* Add it to the list: */
                options << gpConverter->toInternalString(enmOptionType);
                /* Make sure item is included by default: */
                if (!(UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders_Default & enmOptionType))
                    fDefault = false;
            }
            /* If option type disabled: */
            else
            {
                /* Make sure item is excluded by default: */
                if (UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders_Default & enmOptionType)
                    fDefault = false;
            }
            /* Save options: */
            if (!fDefault)
                gEDataManager->setVBoxManagerDetailsPaneElementOptions(DetailsElementType_SF, options);
            else
                gEDataManager->setVBoxManagerDetailsPaneElementOptions(DetailsElementType_SF, QStringList());
        }
    }

    /* UserInterface options: */
    iEnumIndex = smo.indexOfEnumerator("DetailsElementOptionTypeUserInterface");
    if (iEnumIndex != -1)
    {
        bool fDefault = true;
        QStringList options;
        const QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
        for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
        {
            /* Prepare current option type: */
            const UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface enmOptionType =
                static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
            /* Skip invalid and default types: */
            if (   enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface_Invalid
                || enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface_Default)
                continue;
            /* If option type enabled: */
            if (m_fOptionsUserInterface & enmOptionType)
            {
                /* Add it to the list: */
                options << gpConverter->toInternalString(enmOptionType);
                /* Make sure item is included by default: */
                if (!(UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface_Default & enmOptionType))
                    fDefault = false;
            }
            /* If option type disabled: */
            else
            {
                /* Make sure item is excluded by default: */
                if (UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface_Default & enmOptionType)
                    fDefault = false;
            }
            /* Save options: */
            if (!fDefault)
                gEDataManager->setVBoxManagerDetailsPaneElementOptions(DetailsElementType_UI, options);
            else
                gEDataManager->setVBoxManagerDetailsPaneElementOptions(DetailsElementType_UI, QStringList());
        }
    }

    /* Description options: */
    iEnumIndex = smo.indexOfEnumerator("DetailsElementOptionTypeDescription");
    if (iEnumIndex != -1)
    {
        bool fDefault = true;
        QStringList options;
        const QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
        for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
        {
            /* Prepare current option type: */
            const UIExtraDataMetaDefs::DetailsElementOptionTypeDescription enmOptionType =
                static_cast<UIExtraDataMetaDefs::DetailsElementOptionTypeDescription>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
            /* Skip invalid and default types: */
            if (   enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeDescription_Invalid
                || enmOptionType == UIExtraDataMetaDefs::DetailsElementOptionTypeDescription_Default)
                continue;
            /* If option type enabled: */
            if (m_fOptionsDescription & enmOptionType)
            {
                /* Add it to the list: */
                options << gpConverter->toInternalString(enmOptionType);
                /* Make sure item is included by default: */
                if (!(UIExtraDataMetaDefs::DetailsElementOptionTypeDescription_Default & enmOptionType))
                    fDefault = false;
            }
            /* If option type disabled: */
            else
            {
                /* Make sure item is excluded by default: */
                if (UIExtraDataMetaDefs::DetailsElementOptionTypeDescription_Default & enmOptionType)
                    fDefault = false;
            }
            /* Save options: */
            if (!fDefault)
                gEDataManager->setVBoxManagerDetailsPaneElementOptions(DetailsElementType_Description, options);
            else
                gEDataManager->setVBoxManagerDetailsPaneElementOptions(DetailsElementType_Description, QStringList());
        }
    }
}

void UIDetailsModel::saveDetailsCategories()
{
    gEDataManager->setSelectorWindowDetailsElements(m_categories);
}

void UIDetailsModel::saveSettings()
{
    saveDetailsOptions();
    saveDetailsCategories();
}

void UIDetailsModel::cleanupContextMenu()
{
    delete m_pContextMenu;
    m_pContextMenu = 0;
}

void UIDetailsModel::cleanupRoot()
{
    delete m_pRoot;
    m_pRoot = 0;
}

void UIDetailsModel::cleanupScene()
{
    delete m_pScene;
    m_pScene = 0;
}

void UIDetailsModel::cleanup()
{
    /* Cleanup things: */
    saveSettings();
    cleanupContextMenu();
    cleanupRoot();
    cleanupScene();
}

bool UIDetailsModel::processContextMenuEvent(QGraphicsSceneContextMenuEvent *pEvent)
{
    /* Pass preview context menu instead: */
    if (QGraphicsItem *pItem = itemAt(pEvent->scenePos()))
        if (pItem->type() == UIDetailsItemType_Preview)
            return false;

    /* Adjust the menu then show it: */
    m_pContextMenu->resize(m_pContextMenu->minimumSizeHint());
    m_pContextMenu->move(pEvent->screenPos());
    m_pContextMenu->show();

    /* Filter: */
    return true;
}


/*********************************************************************************************************************************
*   Class UIDetailsElementAnimationCallback implementation.                                                                      *
*********************************************************************************************************************************/

UIDetailsElementAnimationCallback::UIDetailsElementAnimationCallback(QObject *pParent, DetailsElementType enmType, bool fToggled)
    : QObject(pParent)
    , m_enmType(enmType)
    , m_fToggled(fToggled)
{
}

void UIDetailsElementAnimationCallback::addNotifier(UIDetailsElement *pItem)
{
    /* Connect notifier: */
    connect(pItem, &UIDetailsElement::sigToggleElementFinished,
            this, &UIDetailsElementAnimationCallback::sltAnimationFinished);
    /* Remember notifier: */
    m_notifiers << pItem;
}

void UIDetailsElementAnimationCallback::sltAnimationFinished()
{
    /* Determine notifier: */
    UIDetailsElement *pItem = qobject_cast<UIDetailsElement*>(sender());
    /* Disconnect notifier: */
    disconnect(pItem, &UIDetailsElement::sigToggleElementFinished,
               this, &UIDetailsElementAnimationCallback::sltAnimationFinished);
    /* Remove notifier: */
    m_notifiers.removeAll(pItem);
    /* Check if we finished: */
    if (m_notifiers.isEmpty())
        emit sigAllAnimationFinished(m_enmType, m_fToggled);
}
