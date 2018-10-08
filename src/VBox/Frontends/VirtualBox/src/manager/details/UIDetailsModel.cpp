/* $Id$ */
/** @file
 * VBox Qt GUI - UIDetailsModel class implementation.
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
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

/* GUI includes: */
# include "UIDetails.h"
# include "UIDetailsModel.h"
# include "UIDetailsGroup.h"
# include "UIDetailsElement.h"
# include "UIExtraDataManager.h"
# include "VBoxGlobal.h"
# include "UIConverter.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIDetailsModel::UIDetailsModel(UIDetails *pParent)
    : QObject(pParent)
    , m_pDetails(pParent)
    , m_pScene(0)
    , m_pRoot(0)
    , m_pAnimationCallback(0)
{
    /* Prepare scene: */
    prepareScene();

    /* Prepare root: */
    prepareRoot();

    /* Load settings: */
    loadSettings();

    /* Register meta-type: */
    qRegisterMetaType<DetailsElementType>();
}

UIDetailsModel::~UIDetailsModel()
{
    /* Save settings: */
    saveSettings();

    /* Cleanup root: */
    cleanupRoot();

    /* Cleanup scene: */
    cleanupScene();
 }

QGraphicsScene* UIDetailsModel::scene() const
{
    return m_pScene;
}

QGraphicsView* UIDetailsModel::paintDevice() const
{
    if (!m_pScene || m_pScene->views().isEmpty())
        return 0;
    return m_pScene->views().first();
}

QGraphicsItem* UIDetailsModel::itemAt(const QPointF &position) const
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
    const int iSceneMargin = data(DetailsModelData_Margin).toInt();
    const QSize viewportSize = paintDevice()->viewport()->size();
    const int iViewportWidth = viewportSize.width() - 2 * iSceneMargin;

    /* Move root: */
    m_pRoot->setPos(iSceneMargin, iSceneMargin);
    /* Resize root: */
    m_pRoot->resize(iViewportWidth, m_pRoot->minimumHeightHint());
    /* Layout root content: */
    m_pRoot->updateLayout();
}

void UIDetailsModel::setItems(const QList<UIVirtualMachineItem*> &items)
{
    m_pRoot->buildGroup(items);
}

void UIDetailsModel::sltHandleViewResize()
{
    /* Relayout: */
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

void UIDetailsModel::sltToggleAnimationFinished(DetailsElementType type, bool fToggled)
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
            if (pElement->elementType() == type)
                pElement->markAnimationFinished();
        }
    }
    /* Update layout: */
    updateLayout();

    /* Update element open/close status: */
    if (m_settings.contains(type))
        m_settings[type] = fToggled;
}

void UIDetailsModel::sltElementTypeToggled()
{
    /* Which item was toggled? */
    QAction *pAction = qobject_cast<QAction*>(sender());
    DetailsElementType type = pAction->data().value<DetailsElementType>();

    /* Toggle element visibility status: */
    if (m_settings.contains(type))
        m_settings.remove(type);
    else
        m_settings[type] = true;

    /* Rebuild group: */
    m_pRoot->rebuildGroup();
}

QVariant UIDetailsModel::data(int iKey) const
{
    switch (iKey)
    {
        case DetailsModelData_Margin: return 0;
        default: break;
    }
    return QVariant();
}

void UIDetailsModel::prepareScene()
{
    m_pScene = new QGraphicsScene(this);
    m_pScene->installEventFilter(this);
}

void UIDetailsModel::prepareRoot()
{
    m_pRoot = new UIDetailsGroup(scene());
}

void UIDetailsModel::loadSettings()
{
    /* Load settings: */
    m_settings = gEDataManager->selectorWindowDetailsElements();
    /* If settings are empty: */
    if (m_settings.isEmpty())
    {
        /* Propose the defaults: */
        m_settings[DetailsElementType_General] = true;
        m_settings[DetailsElementType_Preview] = true;
        m_settings[DetailsElementType_System] = true;
        m_settings[DetailsElementType_Display] = true;
        m_settings[DetailsElementType_Storage] = true;
        m_settings[DetailsElementType_Audio] = true;
        m_settings[DetailsElementType_Network] = true;
        m_settings[DetailsElementType_USB] = true;
        m_settings[DetailsElementType_SF] = true;
        m_settings[DetailsElementType_Description] = true;
    }
}

void UIDetailsModel::saveSettings()
{
    /* Save settings: */
    gEDataManager->setSelectorWindowDetailsElements(m_settings);
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

bool UIDetailsModel::eventFilter(QObject *pObject, QEvent *pEvent)
{
    /* Ignore if no scene object: */
    if (pObject != scene())
        return QObject::eventFilter(pObject, pEvent);

    /* Ignore if no context-menu event: */
    if (pEvent->type() != QEvent::GraphicsSceneContextMenu)
        return QObject::eventFilter(pObject, pEvent);

    /* Process context menu event: */
    return processContextMenuEvent(static_cast<QGraphicsSceneContextMenuEvent*>(pEvent));
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
        DetailsElementType currentElementType = (DetailsElementType)iType;
        QAction *pAction = contextMenu.addAction(gpConverter->toString(currentElementType), this, SLOT(sltElementTypeToggled()));
        pAction->setCheckable(true);
        pAction->setChecked(m_settings.contains(currentElementType));
        pAction->setData(QVariant::fromValue(currentElementType));
    }
    /* Exec context-menu: */
    contextMenu.exec(pEvent->screenPos());

    /* Filter: */
    return true;
}

UIDetailsElementAnimationCallback::UIDetailsElementAnimationCallback(QObject *pParent, DetailsElementType type, bool fToggled)
    : QObject(pParent)
    , m_type(type)
    , m_fToggled(fToggled)
{
}

void UIDetailsElementAnimationCallback::addNotifier(UIDetailsItem *pItem)
{
    /* Connect notifier: */
    connect(pItem, SIGNAL(sigToggleElementFinished()), this, SLOT(sltAnimationFinished()));
    /* Remember notifier: */
    m_notifiers << pItem;
}

void UIDetailsElementAnimationCallback::sltAnimationFinished()
{
    /* Determine notifier: */
    UIDetailsItem *pItem = qobject_cast<UIDetailsItem*>(sender());
    /* Disconnect notifier: */
    disconnect(pItem, SIGNAL(sigToggleElementFinished()), this, SLOT(sltAnimationFinished()));
    /* Remove notifier: */
    m_notifiers.removeAll(pItem);
    /* Check if we finished: */
    if (m_notifiers.isEmpty())
        emit sigAllAnimationFinished(m_type, m_fToggled);
}

