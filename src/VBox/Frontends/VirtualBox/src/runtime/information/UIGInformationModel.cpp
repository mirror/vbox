/* $Id$ */
/** @file
 * VBox Qt GUI - UIGInformationModel class implementation.
 */

/*
 * Copyright (C) 2015 Oracle Corporation
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
# include <QGraphicsScene>
# include <QGraphicsSceneContextMenuEvent>
# include <QGraphicsView>

/* GUI includes: */
# include "UIGInformationModel.h"
# include "UIGInformationGroup.h"
# include "UIGInformationElement.h"
# include "UIExtraDataManager.h"
# include "VBoxGlobal.h"
# include "UIConverter.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIGInformationModel::UIGInformationModel(QObject *pParent)
    : QObject(pParent)
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
    qRegisterMetaType<InformationElementType>();
}

UIGInformationModel::~UIGInformationModel()
{
    /* Save settings: */
    saveSettings();

    /* Cleanup root: */
    cleanupRoot();

    /* Cleanup scene: */
    cleanupScene();
 }

QGraphicsScene* UIGInformationModel::scene() const
{
    return m_pScene;
}

QGraphicsView* UIGInformationModel::paintDevice() const
{
    if (!m_pScene || m_pScene->views().isEmpty())
        return 0;
    return m_pScene->views().first();
}

QGraphicsItem* UIGInformationModel::itemAt(const QPointF &position) const
{
    return scene()->itemAt(position);
}

void UIGInformationModel::updateLayout()
{
    /* Prepare variables: */
    int iSceneMargin = data(DetailsModelData_Margin).toInt();
    QSize viewportSize = paintDevice()->viewport()->size();
    int iViewportWidth = viewportSize.width() - 2 * iSceneMargin;
    int iViewportHeight = viewportSize.height() - 2 * iSceneMargin;

    /* Move root: */
    m_pRoot->setPos(iSceneMargin, iSceneMargin);
    /* Resize root: */
    m_pRoot->resize(iViewportWidth, iViewportHeight);
    /* Layout root content: */
    m_pRoot->updateLayout();
}

void UIGInformationModel::setItems(const QList<UIVMItem*> &items)
{
    m_pRoot->buildGroup(items);
}

QMap<InformationElementType, bool> UIGInformationModel::informationWindowElements()
{
    return m_settings;
}

void UIGInformationModel::setInformationWindowElements(const QMap<InformationElementType, bool> &elements)
{
    //m_settings = elements;
}

void UIGInformationModel::sltHandleViewResize()
{
    /* Relayout: */
    updateLayout();
}

void UIGInformationModel::sltToggleElements(InformationElementType type, bool fToggled)
{
    /* Make sure it is not started yet: */
    if (m_pAnimationCallback)
        return;

    /* Prepare/configure animation callback: */
    m_pAnimationCallback = new UIGInformationElementAnimationCallback(this, type, fToggled);
    connect(m_pAnimationCallback, SIGNAL(sigAllAnimationFinished(InformationElementType, bool)),
            this, SLOT(sltToggleAnimationFinished(InformationElementType, bool)), Qt::QueuedConnection);
    /* For each the set of the group: */
    foreach (UIGInformationItem *pSetItem, m_pRoot->items())
    {
        /* For each the element of the set: */
        foreach (UIGInformationItem *pElementItem, pSetItem->items())
        {
            /* Get each element: */
            UIGInformationElement *pElement = pElementItem->toElement();
            /* Check if this element is of required type: */
            if (pElement->elementType() == type)
            {
                if (fToggled && pElement->closed())
                {
                    m_pAnimationCallback->addNotifier(pElement);
                    pElement->open();
                }
                else if (!fToggled && pElement->opened())
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

void UIGInformationModel::sltToggleAnimationFinished(InformationElementType type, bool fToggled)
{
    /* Cleanup animation callback: */
    delete m_pAnimationCallback;
    m_pAnimationCallback = 0;

    /* Mark animation finished: */
    foreach (UIGInformationItem *pSetItem, m_pRoot->items())
    {
        foreach (UIGInformationItem *pElementItem, pSetItem->items())
        {
            UIGInformationElement *pElement = pElementItem->toElement();
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

void UIGInformationModel::sltElementTypeToggled()
{
    /* Which item was toggled? */
    QAction *pAction = qobject_cast<QAction*>(sender());
    InformationElementType type = pAction->data().value<InformationElementType>();

    /* Toggle element visibility status: */
    if (m_settings.contains(type))
        m_settings.remove(type);
    else
        m_settings[type] = true;

    /* Rebuild group: */
    m_pRoot->rebuildGroup();
}

void UIGInformationModel::sltHandleSlidingStarted()
{
    m_pRoot->stopBuildingGroup();
}

void UIGInformationModel::sltHandleToggleStarted()
{
    m_pRoot->stopBuildingGroup();
}

void UIGInformationModel::sltHandleToggleFinished()
{
    m_pRoot->rebuildGroup();
}

QVariant UIGInformationModel::data(int iKey) const
{
    switch (iKey)
    {
        case DetailsModelData_Margin: return 0;
        default: break;
    }
    return QVariant();
}

void UIGInformationModel::prepareScene()
{
    m_pScene = new QGraphicsScene(this);
    m_pScene->installEventFilter(this);
}

void UIGInformationModel::prepareRoot()
{
    m_pRoot = new UIGInformationGroup(scene());
}

void UIGInformationModel::loadSettings()
{
    /* If settings are empty: */
    //if (m_settings.isEmpty())
    {
        /* Propose the defaults: */
        m_settings[InformationElementType_General] = true;
        m_settings[InformationElementType_System] = true;
        m_settings[InformationElementType_Display] = true;
        m_settings[InformationElementType_Storage] = true;
        m_settings[InformationElementType_Audio] = true;
        m_settings[InformationElementType_Network] = true;
        m_settings[InformationElementType_USB] = true;
        m_settings[InformationElementType_SF] = true;
        m_settings[InformationElementType_Description] = true;
        m_settings[InformationElementType_RuntimeAttributes] = true;
    }
}

void UIGInformationModel::saveSettings()
{
    /* Save settings: */
    //gEDataManager->setInformationWindowElements(m_settings);
}

void UIGInformationModel::cleanupRoot()
{
    delete m_pRoot;
    m_pRoot = 0;
}

void UIGInformationModel::cleanupScene()
{
    delete m_pScene;
    m_pScene = 0;
}

bool UIGInformationModel::eventFilter(QObject *pObject, QEvent *pEvent)
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

bool UIGInformationModel::processContextMenuEvent(QGraphicsSceneContextMenuEvent *pEvent)
{
    /* Pass preview context menu instead: */
    if (QGraphicsItem *pItem = itemAt(pEvent->scenePos()))
        if (pItem->type() == UIGInformationItemType_Preview)
            return false;

    /* Prepare context-menu: */
    QMenu contextMenu;
    /* Enumerate elements settings: */
    for (int iType = InformationElementType_General; iType <= InformationElementType_RuntimeAttributes; ++iType)
    {
        InformationElementType currentElementType = (InformationElementType)iType;
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

UIGInformationElementAnimationCallback::UIGInformationElementAnimationCallback(QObject *pParent, InformationElementType type, bool fToggled)
    : QObject(pParent)
    , m_type(type)
    , m_fToggled(fToggled)
{
}

void UIGInformationElementAnimationCallback::addNotifier(UIGInformationItem *pItem)
{
    /* Connect notifier: */
    connect(pItem, SIGNAL(sigToggleElementFinished()), this, SLOT(sltAnimationFinished()));
    /* Remember notifier: */
    m_notifiers << pItem;
}

void UIGInformationElementAnimationCallback::sltAnimationFinished()
{
    /* Determine notifier: */
    UIGInformationItem *pItem = qobject_cast<UIGInformationItem*>(sender());
    /* Disconnect notifier: */
    disconnect(pItem, SIGNAL(sigToggleElementFinished()), this, SLOT(sltAnimationFinished()));
    /* Remove notifier: */
    m_notifiers.removeAll(pItem);
    /* Check if we finished: */
    if (m_notifiers.isEmpty())
        emit sigAllAnimationFinished(m_type, m_fToggled);
}

