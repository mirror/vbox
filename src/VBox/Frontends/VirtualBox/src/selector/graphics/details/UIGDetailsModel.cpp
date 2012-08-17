/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGDetailsModel class implementation
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

/* Qt includes: */
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsSceneContextMenuEvent>

/* GUI includes: */
#include "UIGDetailsModel.h"
#include "UIGDetailsGroup.h"
#include "UIGDetailsElement.h"
#include "UIGChooserItemGroup.h"
#include "UIGChooserItemMachine.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "UIConverter.h"

/* COM includes: */
#include "CMachine.h"

UIGDetailsModel::UIGDetailsModel(QObject *pParent)
    : QObject(pParent)
    , m_pScene(0)
    , m_pRoot(0)
    , m_pAnimationCallback(0)
{
    /* Prepare scene: */
    prepareScene();

    /* Prepare root: */
    prepareRoot();

    /* Register meta-type: */
    qRegisterMetaType<DetailsElementType>();
}

UIGDetailsModel::~UIGDetailsModel()
{
    /* Cleanup root: */
    cleanupRoot();

    /* Cleanup scene: */
    cleanupScene();
 }

QGraphicsScene* UIGDetailsModel::scene() const
{
    return m_pScene;
}

QGraphicsItem* UIGDetailsModel::itemAt(const QPointF &position, const QTransform &deviceTransform /* = QTransform() */) const
{
    return scene()->itemAt(position, deviceTransform);
}

void UIGDetailsModel::setItems(const QList<UIVMItem*> &items)
{
    m_pRoot->setItems(items);
}

void UIGDetailsModel::updateLayout()
{
    /* Initialize variables: */
    int iSceneMargin = data(DetailsModelData_Margin).toInt();
    QSize viewportSize = scene()->views()[0]->viewport()->size();
    int iViewportWidth = viewportSize.width() - 2 * iSceneMargin;
    int iViewportHeight = viewportSize.height() - 2 * iSceneMargin;
    /* Set root item position: */
    m_pRoot->setPos(iSceneMargin, iSceneMargin);
    /* Set root item size: */
    m_pRoot->resize(iViewportWidth, iViewportHeight);
    /* Relayout root item: */
    m_pRoot->updateLayout();
    /* Notify listener about root-item relayouted: */
    emit sigRootItemResized(m_pRoot->geometry().size(), m_pRoot->minimumSizeHint().toSize().width());
}

void UIGDetailsModel::sltHandleViewResized()
{
    /* Relayout: */
    updateLayout();
}

void UIGDetailsModel::sltToggleElements(DetailsElementType type, bool fToggled)
{
    /* Make sure not started yet: */
    if (m_pAnimationCallback)
        return;
    /* Prepare/configure animation callback: */
    m_pAnimationCallback = new UIGDetailsElementAnimationCallback(this, type, fToggled);
    connect(m_pAnimationCallback, SIGNAL(sigAllAnimationFinished(DetailsElementType, bool)),
            this, SLOT(sltToggleAnimationFinished(DetailsElementType, bool)), Qt::QueuedConnection);
    /* For each the set of the group: */
    foreach (UIGDetailsItem *pSetItem, m_pRoot->items())
    {
        /* For each the element of the set: */
        foreach (UIGDetailsItem *pElementItem, pSetItem->items())
        {
            /* Get each element: */
            UIGDetailsElement *pElement = pElementItem->toElement();
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

void UIGDetailsModel::sltToggleAnimationFinished(DetailsElementType type, bool fToggled)
{
    /* Cleanup animation callback: */
    delete m_pAnimationCallback;
    m_pAnimationCallback = 0;

    /* Mark animation finished: */
    foreach (UIGDetailsItem *pSetItem, m_pRoot->items())
    {
        foreach (UIGDetailsItem *pElementItem, pSetItem->items())
        {
            UIGDetailsElement *pElement = pElementItem->toElement();
            if (pElement->elementType() == type)
                pElement->markAnimationFinished();
        }
    }
    /* Update layout: */
    updateLayout();

    /* Update details settings: */
    QStringList detailsSettings = vboxGlobal().virtualBox().GetExtraDataStringList(GUI_DetailsPageBoxes);
    QString strOldElementName = gpConverter->toInternalString(type);
    QString strNewElementName = strOldElementName;
    if (fToggled)
        strOldElementName += "Closed";
    else
        strNewElementName += "Closed";
    int iIndex = detailsSettings.indexOf(strOldElementName);
    if (iIndex != -1)
    {
        detailsSettings[iIndex] = strNewElementName;
        vboxGlobal().virtualBox().SetExtraDataStringList(GUI_DetailsPageBoxes, detailsSettings);
    }
}

void UIGDetailsModel::sltElementTypeToggled()
{
    /* Which item was toggled? */
    QAction *pAction = qobject_cast<QAction*>(sender());
    DetailsElementType elementType = pAction->data().value<DetailsElementType>();
    QString strElementTypeOpened = gpConverter->toInternalString(elementType);
    QString strElementTypeClosed = strElementTypeOpened + "Closed";
    QStringList detailsSettings = vboxGlobal().virtualBox().GetExtraDataStringList(GUI_DetailsPageBoxes);
    /* Update details settings: */
    bool fElementExists = detailsSettings.contains(strElementTypeOpened) ||
                          detailsSettings.contains(strElementTypeClosed);
    if (fElementExists)
    {
        detailsSettings.removeAll(strElementTypeOpened);
        detailsSettings.removeAll(strElementTypeClosed);
    }
    else
    {
        detailsSettings.append(strElementTypeClosed);
    }
    vboxGlobal().virtualBox().SetExtraDataStringList(GUI_DetailsPageBoxes, detailsSettings);
    m_pRoot->updateItems();
}

void UIGDetailsModel::sltHandleSlidingStarted()
{
    m_pRoot->stopPopulatingItems();
}

void UIGDetailsModel::sltHandleToggleStarted()
{
    m_pRoot->stopPopulatingItems();
}

void UIGDetailsModel::sltHandleToggleFinished()
{
    m_pRoot->updateItems();
}

QVariant UIGDetailsModel::data(int iKey) const
{
    switch (iKey)
    {
        case DetailsModelData_Margin: return 0;
        default: break;
    }
    return QVariant();
}

void UIGDetailsModel::prepareScene()
{
    m_pScene = new QGraphicsScene(this);
    m_pScene->installEventFilter(this);
}

void UIGDetailsModel::prepareRoot()
{
    m_pRoot = new UIGDetailsGroup;
    scene()->addItem(m_pRoot);
}

void UIGDetailsModel::cleanupRoot()
{
    delete m_pRoot;
    m_pRoot = 0;
}

void UIGDetailsModel::cleanupScene()
{
    delete m_pScene;
    m_pScene = 0;
}

bool UIGDetailsModel::eventFilter(QObject *pObject, QEvent *pEvent)
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

bool UIGDetailsModel::processContextMenuEvent(QGraphicsSceneContextMenuEvent *pEvent)
{
    /* Pass preview context menu instead: */
    if (QGraphicsItem *pItem = itemAt(pEvent->scenePos()))
        if (pItem->type() == UIGDetailsItemType_Preview)
            return false;

    /* Prepare context-menu: */
    QMenu contextMenu;
    QStringList detailsSettings = vboxGlobal().virtualBox().GetExtraDataStringList(GUI_DetailsPageBoxes);
    for (int iType = DetailsElementType_General; iType <= DetailsElementType_Description; ++iType)
    {
        DetailsElementType currentElementType = (DetailsElementType)iType;
        QAction *pAction = contextMenu.addAction(gpConverter->toString(currentElementType), this, SLOT(sltElementTypeToggled()));
        pAction->setCheckable(true);
        QString strTypeIdOpened = gpConverter->toInternalString(currentElementType);
        QString strTypeIdClosed = strTypeIdOpened + "Closed";
        pAction->setChecked(detailsSettings.contains(strTypeIdOpened) || detailsSettings.contains(strTypeIdClosed));
        pAction->setData(QVariant::fromValue(currentElementType));
    }
    /* Exec context-menu: */
    contextMenu.exec(pEvent->screenPos());

    /* Filter: */
    return true;
}

UIGDetailsElementAnimationCallback::UIGDetailsElementAnimationCallback(QObject *pParent, DetailsElementType type, bool fToggled)
    : QObject(pParent)
    , m_type(type)
    , m_fToggled(fToggled)
{
}

void UIGDetailsElementAnimationCallback::addNotifier(UIGDetailsItem *pItem)
{
    /* Connect notifier: */
    connect(pItem, SIGNAL(sigToggleElementFinished()), this, SLOT(sltAnimationFinished()));
    /* Remember notifier: */
    m_notifiers << pItem;
}

void UIGDetailsElementAnimationCallback::sltAnimationFinished()
{
    /* Determine notifier: */
    UIGDetailsItem *pItem = qobject_cast<UIGDetailsItem*>(sender());
    /* Disconnect notifier: */
    disconnect(pItem, SIGNAL(sigToggleElementFinished()), this, SLOT(sltAnimationFinished()));
    /* Remove notifier: */
    m_notifiers.removeAll(pItem);
    /* Check if we finished: */
    if (m_notifiers.isEmpty())
        emit sigAllAnimationFinished(m_type, m_fToggled);
}

