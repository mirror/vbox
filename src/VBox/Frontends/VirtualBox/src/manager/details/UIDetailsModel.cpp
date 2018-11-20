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
# include <QMenu>
# include <QMetaEnum>

/* GUI includes: */
# include "UIConverter.h"
# include "UIDetails.h"
# include "UIDetailsModel.h"
# include "UIDetailsGroup.h"
# include "UIDetailsElement.h"
# include "UIExtraDataManager.h"
# include "VBoxGlobal.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   Class UIDetailsModel implementation.                                                                                         *
*********************************************************************************************************************************/

UIDetailsModel::UIDetailsModel(UIDetails *pParent)
    : QObject(pParent)
    , m_pDetails(pParent)
    , m_pScene(0)
    , m_pRoot(0)
    , m_pAnimationCallback(0)
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
}

void UIDetailsModel::setOptionsGeneral(UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral fOptionsGeneral)
{
    m_fOptionsGeneral = fOptionsGeneral;
    m_pRoot->rebuildGroup();
}

void UIDetailsModel::setOptionsSystem(UIExtraDataMetaDefs::DetailsElementOptionTypeSystem fOptionsSystem)
{
    m_fOptionsSystem = fOptionsSystem;
    m_pRoot->rebuildGroup();
}

void UIDetailsModel::setOptionsDisplay(UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay fOptionsDisplay)
{
    m_fOptionsDisplay = fOptionsDisplay;
    m_pRoot->rebuildGroup();
}

void UIDetailsModel::setOptionsStorage(UIExtraDataMetaDefs::DetailsElementOptionTypeStorage fOptionsStorage)
{
    m_fOptionsStorage = fOptionsStorage;
    m_pRoot->rebuildGroup();
}

void UIDetailsModel::setOptionsAudio(UIExtraDataMetaDefs::DetailsElementOptionTypeAudio fOptionsAudio)
{
    m_fOptionsAudio = fOptionsAudio;
    m_pRoot->rebuildGroup();
}

void UIDetailsModel::setOptionsNetwork(UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork fOptionsNetwork)
{
    m_fOptionsNetwork = fOptionsNetwork;
    m_pRoot->rebuildGroup();
}

void UIDetailsModel::setOptionsSerial(UIExtraDataMetaDefs::DetailsElementOptionTypeSerial fOptionsSerial)
{
    m_fOptionsSerial = fOptionsSerial;
    m_pRoot->rebuildGroup();
}

void UIDetailsModel::setOptionsUsb(UIExtraDataMetaDefs::DetailsElementOptionTypeUsb fOptionsUsb)
{
    m_fOptionsUsb = fOptionsUsb;
    m_pRoot->rebuildGroup();
}

void UIDetailsModel::setOptionsSharedFolders(UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders fOptionsSharedFolders)
{
    m_fOptionsSharedFolders = fOptionsSharedFolders;
    m_pRoot->rebuildGroup();
}

void UIDetailsModel::setOptionsUserInterface(UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface fOptionsUserInterface)
{
    m_fOptionsUserInterface = fOptionsUserInterface;
    m_pRoot->rebuildGroup();
}

void UIDetailsModel::setOptionsDescription(UIExtraDataMetaDefs::DetailsElementOptionTypeDescription fOptionsDescription)
{
    m_fOptionsDescription = fOptionsDescription;
    m_pRoot->rebuildGroup();
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
    m_pRoot->rebuildGroup();
}

void UIDetailsModel::sltHandleExtraDataOptionsChange(DetailsElementType enmType)
{
    loadDetailsOptions(enmType);
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

void UIDetailsModel::sltElementTypeToggled()
{
    /* Which item was toggled? */
    QAction *pAction = qobject_cast<QAction*>(sender());
    DetailsElementType enmType = pAction->data().value<DetailsElementType>();

    /* Toggle element visibility status: */
    if (m_categories.contains(enmType))
        m_categories.remove(enmType);
    else
        m_categories[enmType] = true;

    /* Rebuild group: */
    m_pRoot->rebuildGroup();
}

void UIDetailsModel::prepare()
{
    /* Prepare things: */
    prepareScene();
    prepareRoot();
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

void UIDetailsModel::loadSettings()
{
    loadDetailsCategories();
    loadDetailsOptions();
}

void UIDetailsModel::loadDetailsCategories()
{
    m_categories = gEDataManager->selectorWindowDetailsElements();
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
    cleanupRoot();
    cleanupScene();
}

bool UIDetailsModel::processContextMenuEvent(QGraphicsSceneContextMenuEvent *pEvent)
{
    /* Pass preview context menu instead: */
    if (QGraphicsItem *pItem = itemAt(pEvent->scenePos()))
        if (pItem->type() == UIDetailsItemType_Preview)
            return false;

    /* Prepare context-menu: */
    QMenu contextMenu;
    /* Enumerate elements settings: */
    for (int iType = DetailsElementType_General; iType <= DetailsElementType_Description; ++iType)
    {
        const DetailsElementType enmCurrentElementType = (DetailsElementType)iType;
        QAction *pAction = contextMenu.addAction(gpConverter->toString(enmCurrentElementType), this, SLOT(sltElementTypeToggled()));
        pAction->setCheckable(true);
        pAction->setChecked(m_categories.contains(enmCurrentElementType));
        pAction->setData(QVariant::fromValue(enmCurrentElementType));
    }
    /* Exec context-menu: */
    contextMenu.exec(pEvent->screenPos());

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
