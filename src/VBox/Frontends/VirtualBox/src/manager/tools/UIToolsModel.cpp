/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolsModel class implementation.
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
# include <QGraphicsScene>
# include <QGraphicsSceneContextMenuEvent>
# include <QGraphicsSceneMouseEvent>
# include <QGraphicsView>
# include <QPropertyAnimation>
# include <QScrollBar>
# include <QRegExp>
# include <QTimer>

/* GUI includes: */
# include "QIMessageBox.h"
# include "VBoxGlobal.h"
# include "UIActionPoolSelector.h"
# include "UIIconPool.h"
# include "UITools.h"
# include "UIToolsHandlerMouse.h"
# include "UIToolsHandlerKeyboard.h"
# include "UIToolsModel.h"
# include "UIExtraDataDefs.h"
# include "UIExtraDataManager.h"
# include "UIMessageCenter.h"
# include "UIModalWindowManager.h"
# include "UIVirtualBoxManagerWidget.h"
# include "UIVirtualBoxEventHandler.h"
# include "UIWizardNewVM.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
#include <QParallelAnimationGroup>

/* Type defs: */
typedef QSet<QString> UIStringSet;


UIToolsModel:: UIToolsModel(UITools *pParent)
    : QObject(pParent)
    , m_pTools(pParent)
    , m_pScene(0)
    , m_pMouseHandler(0)
    , m_pKeyboardHandler(0)
    , m_enmCurrentClass(UIToolsClass_Global)
{
    /* Prepare: */
    prepare();
}

UIToolsModel::~UIToolsModel()
{
    /* Cleanup: */
    cleanup();
}

void UIToolsModel::init()
{
    /* Load last selected item: */
    loadLastSelectedItems();

    /* Update linked values: */
    updateLayout();
    updateNavigation();
    sltItemMinimumWidthHintChanged();
    sltItemMinimumHeightHintChanged();
}

void UIToolsModel::deinit()
{
    /* Save last selected item: */
    saveLastSelectedItems();
}

UITools *UIToolsModel::tools() const
{
    return m_pTools;
}

UIActionPool *UIToolsModel::actionPool() const
{
    return tools()->actionPool();
}

QGraphicsScene *UIToolsModel::scene() const
{
    return m_pScene;
}

QPaintDevice *UIToolsModel::paintDevice() const
{
    if (scene() && !scene()->views().isEmpty())
        return scene()->views().first();
    return 0;
}

QGraphicsItem *UIToolsModel::itemAt(const QPointF &position, const QTransform &deviceTransform /* = QTransform() */) const
{
    return scene()->itemAt(position, deviceTransform);
}

void UIToolsModel::setToolsClass(UIToolsClass enmClass)
{
    /* Update linked values: */
    if (m_enmCurrentClass != enmClass)
    {
        m_enmCurrentClass = enmClass;
        updateLayout();
        updateNavigation();
    }
}

UIToolsClass UIToolsModel::toolsClass() const
{
    return m_enmCurrentClass;
}

UIToolsType UIToolsModel::toolsType() const
{
    return currentItem()->itemType();
}

void UIToolsModel::setToolsEnabled(UIToolsClass enmClass, bool fEnabled)
{
    /* Update linked values: */
    m_statesToolsEnabled[enmClass] = fEnabled;
    foreach (UIToolsItem *pItem, items())
        if (pItem->itemClass() == enmClass)
            pItem->setEnabled(m_statesToolsEnabled.value(enmClass));
}

bool UIToolsModel::areToolsEnabled(UIToolsClass enmClass) const
{
    return m_statesToolsEnabled.value(enmClass);
}

void UIToolsModel::setCurrentItem(UIToolsItem *pItem)
{
    /* Is there something changed? */
    if (m_pCurrentItem == pItem)
        return;

    /* Remember old current-item: */
    UIToolsItem *pOldCurrentItem = m_pCurrentItem;

    /* If there is item: */
    if (pItem)
    {
        /* Set this item to current if navigation list contains it: */
        if (navigationList().contains(pItem))
        {
            m_pCurrentItem = pItem;
            switch (m_pCurrentItem->itemClass())
            {
                case UIToolsClass_Global:  m_pLastItemGlobal  = m_pCurrentItem; break;
                case UIToolsClass_Machine: m_pLastItemMachine = m_pCurrentItem; break;
                default: break;
            }
        }
        /* Otherwise it's error: */
        else
            AssertMsgFailed(("Passed item is not in navigation list!"));
    }
    /* Otherwise reset current item: */
    else
        m_pCurrentItem = 0;

    /* Update old item (if any): */
    if (pOldCurrentItem)
        pOldCurrentItem->update();
    /* Update new item (if any): */
    if (m_pCurrentItem)
        m_pCurrentItem->update();

    /* Notify about selection change: */
    emit sigSelectionChanged();

    /* Move focus to current-item: */
    setFocusItem(currentItem());
}

UIToolsItem *UIToolsModel::currentItem() const
{
    return m_pCurrentItem;
}

void UIToolsModel::setFocusItem(UIToolsItem *pItem)
{
    /* Always make sure real focus unset: */
    scene()->setFocusItem(0);

    /* Is there something changed? */
    if (m_pFocusItem == pItem)
        return;

    /* Remember old focus-item: */
    UIToolsItem *pOldFocusItem = m_pFocusItem;

    /* If there is item: */
    if (pItem)
    {
        /* Set this item to focus if navigation list contains it: */
        if (navigationList().contains(pItem))
            m_pFocusItem = pItem;
        /* Otherwise it's error: */
        else
            AssertMsgFailed(("Passed item is not in navigation list!"));
    }
    /* Otherwise reset focus item: */
    else
        m_pFocusItem = 0;

    /* Disconnect old focus-item (if any): */
    if (pOldFocusItem)
        disconnect(pOldFocusItem, SIGNAL(destroyed(QObject*)), this, SLOT(sltFocusItemDestroyed()));
    /* Connect new focus-item (if any): */
    if (m_pFocusItem)
        connect(m_pFocusItem, SIGNAL(destroyed(QObject*)), this, SLOT(sltFocusItemDestroyed()));

    /* Notify about focus change: */
    emit sigFocusChanged();
}

UIToolsItem *UIToolsModel::focusItem() const
{
    return m_pFocusItem;
}

void UIToolsModel::makeSureSomeItemIsSelected()
{
    /* Make sure selection item list is never empty
     * if at least one item (for example 'focus') present: */
    if (!currentItem() && focusItem())
        setCurrentItem(focusItem());
}

const QList<UIToolsItem*> &UIToolsModel::navigationList() const
{
    return m_navigationList;
}

void UIToolsModel::removeFromNavigationList(UIToolsItem *pItem)
{
    AssertMsg(pItem, ("Passed item is invalid!"));
    m_navigationList.removeAll(pItem);
}

void UIToolsModel::updateNavigation()
{
    /* Clear list initially: */
    m_navigationList.clear();

    /* Enumerate the children: */
    foreach (UIToolsItem *pItem, items())
        if (pItem->isVisible())
            m_navigationList << pItem;

    /* Choose last selected item of current class: */
    UIToolsItem *pLastSelectedItem = m_enmCurrentClass == UIToolsClass_Global
                                   ? m_pLastItemGlobal : m_pLastItemMachine;
    if (navigationList().contains(pLastSelectedItem))
        setCurrentItem(pLastSelectedItem);
}

QList<UIToolsItem*> UIToolsModel::items() const
{
    return m_items;
}

void UIToolsModel::updateLayout()
{
    /* Initialize variables: */
    const QSize viewportSize = scene()->views()[0]->viewport()->size();
    const int iViewportWidth = viewportSize.width();
    int iVerticalIndent = 0;

    /* Layout the children: */
    foreach (UIToolsItem *pItem, items())
    {
        /* Hide/skip unrelated items: */
        if (pItem->itemClass() != m_enmCurrentClass)
        {
            pItem->hide();
            continue;
        }

        /* Set item position: */
        pItem->setPos(0, iVerticalIndent);
        /* Set root-item size: */
        pItem->resize(iViewportWidth, pItem->minimumHeightHint());
        /* Make sure item is shown: */
        pItem->show();
        /* Advance vertical indent: */
        iVerticalIndent += pItem->minimumHeightHint();
    }
}

void UIToolsModel::sltHandleViewResized()
{
    /* Relayout: */
    updateLayout();
}

void UIToolsModel::sltItemMinimumWidthHintChanged()
{
    /* Calculate maximum horizontal width: */
    int iMinimumWidthHint = 0;
    foreach (UIToolsItem *pItem, items())
        iMinimumWidthHint = qMax(iMinimumWidthHint, pItem->minimumWidthHint());
    emit sigItemMinimumWidthHintChanged(iMinimumWidthHint);
}

void UIToolsModel::sltItemMinimumHeightHintChanged()
{
    /* Calculate summary vertical height: */
    int iMinimumHeightHint = 0;
    foreach (UIToolsItem *pItem, items())
        iMinimumHeightHint += pItem->minimumHeightHint();
    emit sigItemMinimumHeightHintChanged(iMinimumHeightHint);
}

bool UIToolsModel::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* Process only scene events: */
    if (pWatched != scene())
        return QObject::eventFilter(pWatched, pEvent);

    /* Process only item focused by model: */
    if (scene()->focusItem())
        return QObject::eventFilter(pWatched, pEvent);

    /* Do not handle disabled items: */
    if (!currentItem()->isEnabled())
        return QObject::eventFilter(pWatched, pEvent);

    /* Checking event-type: */
    switch (pEvent->type())
    {
        /* Keyboard handler: */
        case QEvent::KeyPress:
            return m_pKeyboardHandler->handle(static_cast<QKeyEvent*>(pEvent), UIKeyboardEventType_Press);
        case QEvent::KeyRelease:
            return m_pKeyboardHandler->handle(static_cast<QKeyEvent*>(pEvent), UIKeyboardEventType_Release);
        /* Mouse handler: */
        case QEvent::GraphicsSceneMousePress:
            return m_pMouseHandler->handle(static_cast<QGraphicsSceneMouseEvent*>(pEvent), UIMouseEventType_Press);
        case QEvent::GraphicsSceneMouseRelease:
            return m_pMouseHandler->handle(static_cast<QGraphicsSceneMouseEvent*>(pEvent), UIMouseEventType_Release);
        /* Shut up MSC: */
        default: break;
    }

    /* Call to base-class: */
    return QObject::eventFilter(pWatched, pEvent);
}

void UIToolsModel::sltFocusItemDestroyed()
{
    AssertMsgFailed(("Focus item destroyed!"));
}

void UIToolsModel::prepare()
{
    /* Prepare scene: */
    prepareScene();
    /* Prepare items: */
    prepareItems();
    /* Prepare handlers: */
    prepareHandlers();
    /* Prepare connections: */
    prepareConnections();
}

void UIToolsModel::prepareScene()
{
    m_pScene = new QGraphicsScene(this);
    if (m_pScene)
        m_pScene->installEventFilter(this);
}

void UIToolsModel::prepareItems()
{
    /* Enable all classes of tools initially: */
    m_statesToolsEnabled[UIToolsClass_Global] = true;
    m_statesToolsEnabled[UIToolsClass_Machine] = true;

    /* Prepare classes: */
    QList<UIToolsClass> classes;
    classes << UIToolsClass_Global;
    classes << UIToolsClass_Global;
    classes << UIToolsClass_Machine;
    classes << UIToolsClass_Machine;

    /* Prepare types: */
    QList<UIToolsType> types;
    types << UIToolsType_Media;
    types << UIToolsType_Network;
    types << UIToolsType_Details;
    types << UIToolsType_Snapshots;

    /* Prepare icons: */
    QList<QIcon> icons;
    icons << UIIconPool::iconSet(":/media_manager_22px.png");
    icons << UIIconPool::iconSet(":/host_iface_manager_22px.png");
    icons << UIIconPool::iconSet(":/machine_details_manager_22px.png");
    icons << UIIconPool::iconSet(":/snapshot_manager_22px.png");

    /* Prepare names: */
    QList<QString> names;
    names << tr("Media");
    names << tr("Network");
    names << tr("Details");
    names << tr("Snapshots");

    /* Populate the items: */
    for (int i = 0; i < names.size(); ++i)
        m_items << new UIToolsItem(scene(),
                                   classes.value(i),
                                   types.value(i),
                                   icons.value(i),
                                   names.value(i));
}

void UIToolsModel::prepareHandlers()
{
    m_pMouseHandler = new UIToolsHandlerMouse(this);
    m_pKeyboardHandler = new UIToolsHandlerKeyboard(this);
}

void UIToolsModel::prepareConnections()
{
    /* Setup parent connections: */
    connect(this, SIGNAL(sigSelectionChanged()),
            parent(), SIGNAL(sigSelectionChanged()));
    connect(this, SIGNAL(sigExpandingStarted()),
            parent(), SIGNAL(sigExpandingStarted()));
    connect(this, SIGNAL(sigExpandingFinished()),
            parent(), SIGNAL(sigExpandingFinished()));
}

void UIToolsModel::loadLastSelectedItems()
{
    /* Load selected items data: */
    const QString strData = gEDataManager->toolsPaneLastItemsChosen();

    /* Split serialized data to pair of values: */
    const QStringList values = strData.split(",");

    /* First of them is current global class item definition: */
    UIToolsType enmTypeGlobal = typeFromString(values.value(0));
    if (!isTypeOfClass(enmTypeGlobal, UIToolsClass_Global))
        enmTypeGlobal = UIToolsType_Media;
    foreach (UIToolsItem *pItem, items())
        if (pItem->itemType() == enmTypeGlobal)
            m_pLastItemGlobal = pItem;
    AssertPtr(m_pLastItemGlobal.data());

    /* Second of them is current machine class item definition: */
    UIToolsType enmTypeMachine = typeFromString(values.value(1));
    if (!isTypeOfClass(enmTypeMachine, UIToolsClass_Machine))
        enmTypeMachine = UIToolsType_Details;
    foreach (UIToolsItem *pItem, items())
        if (pItem->itemType() == enmTypeMachine)
            m_pLastItemMachine = pItem;
    AssertPtr(m_pLastItemMachine.data());
}

void UIToolsModel::saveLastSelectedItems()
{
    /* Prepare selected items data: */
    const QString strData = QString("%1,%2")
          .arg(typeToString(m_pLastItemGlobal->itemType()))
          .arg(typeToString(m_pLastItemMachine->itemType()));

    /* Save selected items data: */
    gEDataManager->setToolsPaneLastItemsChosen(strData);
}

void UIToolsModel::cleanupHandlers()
{
    delete m_pKeyboardHandler;
    m_pKeyboardHandler = 0;
    delete m_pMouseHandler;
    m_pMouseHandler = 0;
}

void UIToolsModel::cleanupItems()
{
    foreach (UIToolsItem *pItem, m_items)
        delete pItem;
    m_items.clear();
}

void UIToolsModel::cleanupScene()
{
    delete m_pScene;
    m_pScene = 0;
}

void UIToolsModel::cleanup()
{
    /* Cleanup handlers: */
    cleanupHandlers();
    /* Cleanup items: */
    cleanupItems();
    /* Cleanup scene: */
    cleanupScene();
}

/* static */
QString UIToolsModel::typeToString(UIToolsType enmType)
{
    switch (enmType)
    {
        /* Global classes: */
        case UIToolsType_Media:     return "Media";
        case UIToolsType_Network:   return "Network";
        /* Machine classes: */
        case UIToolsType_Details:   return "Details";
        case UIToolsType_Snapshots: return "Snapshots";
        case UIToolsType_Logs:      return "Logs";
        default: break;
    }
    return QString();
}

/* static */
UIToolsType UIToolsModel::typeFromString(const QString &strType)
{
    /* Global classes: */
    if      (strType == "Media")     return UIToolsType_Media;
    else if (strType == "Network")   return UIToolsType_Network;
    /* Machine classes: */
    else if (strType == "Details")   return UIToolsType_Details;
    else if (strType == "Snapshots") return UIToolsType_Snapshots;
    else if (strType == "Logs")      return UIToolsType_Logs;
    return UIToolsType_Invalid;
}

/* static */
bool UIToolsModel::isTypeOfClass(UIToolsType enmType, UIToolsClass enmClass)
{
    switch (enmClass)
    {
        case UIToolsClass_Global:
        {
            switch (enmType)
            {
                case UIToolsType_Media:
                case UIToolsType_Network:
                    return true;
                default:
                    break;
            }
            break;
        }
        case UIToolsClass_Machine:
        {
            switch (enmType)
            {
                case UIToolsType_Details:
                case UIToolsType_Snapshots:
                case UIToolsType_Logs:
                    return true;
                default:
                    break;
            }
            break;
        }
        default:
            break;
    }
    return false;
}
