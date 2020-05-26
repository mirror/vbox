/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooser class implementation.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
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
#include <QVBoxLayout>

/* GUI includes: */
#include "UICommon.h"
#include "UIChooser.h"
#include "UIChooserModel.h"
#include "UIChooserView.h"
#include "UIVirtualBoxManagerWidget.h"


UIChooser::UIChooser(UIVirtualBoxManagerWidget *pParent)
    : QWidget(pParent)
    , m_pManagerWidget(pParent)
    , m_pChooserModel(0)
    , m_pChooserView(0)
{
    prepare();
}

UIChooser::~UIChooser()
{
    cleanup();
}

UIActionPool *UIChooser::actionPool() const
{
    AssertPtrReturn(managerWidget(), 0);
    return managerWidget()->actionPool();
}

UIVirtualMachineItem *UIChooser::currentItem() const
{
    AssertPtrReturn(model(), 0);
    return model()->firstSelectedMachineItem();
}

QList<UIVirtualMachineItem*> UIChooser::currentItems() const
{
    AssertPtrReturn(model(), QList<UIVirtualMachineItem*>());
    return model()->selectedMachineItems();
}

bool UIChooser::isGroupItemSelected() const
{
    AssertPtrReturn(model(), false);
    return model()->isGroupItemSelected();
}

bool UIChooser::isGlobalItemSelected() const
{
    AssertPtrReturn(model(), false);
    return model()->isGlobalItemSelected();
}

bool UIChooser::isMachineItemSelected() const
{
    AssertPtrReturn(model(), false);
    return model()->isMachineItemSelected();
}

bool UIChooser::isSingleGroupSelected() const
{
    AssertPtrReturn(model(), false);
    return model()->isSingleGroupSelected();
}

bool UIChooser::isSingleLocalGroupSelected() const
{
    AssertPtrReturn(model(), false);
    return model()->isSingleLocalGroupSelected();
}

bool UIChooser::isSingleCloudProfileGroupSelected() const
{
    AssertPtrReturn(model(), false);
    return model()->isSingleCloudProfileGroupSelected();
}

bool UIChooser::isAllItemsOfOneGroupSelected() const
{
    AssertPtrReturn(model(), false);
    return model()->isAllItemsOfOneGroupSelected();
}

QString UIChooser::fullGroupName() const
{
    AssertPtrReturn(model(), QString());
    return model()->fullGroupName();
}

bool UIChooser::isGroupSavingInProgress() const
{
    AssertPtrReturn(model(), false);
    return model()->isGroupSavingInProgress();
}

void UIChooser::openGroupNameEditor()
{
    AssertPtrReturnVoid(model());
    model()->startEditingSelectedGroupItemName();
}

void UIChooser::disbandGroup()
{
    AssertPtrReturnVoid(model());
    model()->disbandSelectedGroupItem();
}

void UIChooser::removeMachine()
{
    AssertPtrReturnVoid(model());
    model()->removeSelectedMachineItems();
}

void UIChooser::moveMachineToGroup(const QString &strName)
{
    AssertPtrReturnVoid(model());
    model()->moveSelectedMachineItemsToGroupItem(strName);
}

void UIChooser::refreshMachine()
{
    AssertPtrReturnVoid(model());
    model()->refreshSelectedMachineItems();
}

void UIChooser::sortGroup()
{
    AssertPtrReturnVoid(model());
    model()->sortSelectedGroupItem();
}

void UIChooser::setMachineSearchWidgetVisibility(bool fVisible)
{
    AssertPtrReturnVoid(view());
    view()->setSearchWidgetVisible(fVisible);
}

void UIChooser::sltHandleToolbarResize(const QSize &newSize)
{
    /* Pass height to a model: */
    AssertPtrReturnVoid(model());
    model()->setGlobalItemHeightHint(newSize.height());
}

void UIChooser::sltToolMenuRequested(UIToolClass enmClass, const QPoint &position)
{
    /* Translate scene coordinates to global one: */
    AssertPtrReturnVoid(view());
    emit sigToolMenuRequested(enmClass, mapToGlobal(view()->mapFromScene(position)));
}

void UIChooser::prepare()
{
    /* Prepare everything: */
    preparePalette();
    prepareModel();
    prepareWidgets();
    prepareConnections();

    /* Init model: */
    initModel();
}

void UIChooser::preparePalette()
{
    setAutoFillBackground(true);
    QPalette pal = palette();
    QColor bodyColor = pal.color(QPalette::Active, QPalette::Midlight).darker(110);
    pal.setColor(QPalette::Window, bodyColor);
    setPalette(pal);
}

void UIChooser::prepareModel()
{
    m_pChooserModel = new UIChooserModel(this);
}

void UIChooser::prepareWidgets()
{
    /* Prepare main-layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    if (pMainLayout)
    {
        pMainLayout->setContentsMargins(0, 0, 0, 0);
        pMainLayout->setSpacing(0);

        /* Prepare chooser-view: */
        m_pChooserView = new UIChooserView(this);
        if (m_pChooserView)
        {
            AssertPtrReturnVoid(model());
            m_pChooserView->setScene(model()->scene());
            m_pChooserView->show();
            setFocusProxy(m_pChooserView);

            /* Add into layout: */
            pMainLayout->addWidget(m_pChooserView);
        }
    }
}

void UIChooser::prepareConnections()
{
    AssertPtrReturnVoid(model());
    AssertPtrReturnVoid(view());

    /* Chooser-model connections: */
    connect(model(), &UIChooserModel::sigGroupSavingStateChanged,
            this, &UIChooser::sigGroupSavingStateChanged);
    connect(model(), &UIChooserModel::sigSelectionChanged,
            this, &UIChooser::sigSelectionChanged);
    connect(model(), &UIChooserModel::sigSelectionInvalidated,
            this, &UIChooser::sigSelectionInvalidated);
    connect(model(), &UIChooserModel::sigToggleStarted,
            this, &UIChooser::sigToggleStarted);
    connect(model(), &UIChooserModel::sigToggleFinished,
            this, &UIChooser::sigToggleFinished);
    connect(model(), &UIChooserModel::sigRootItemMinimumWidthHintChanged,
            view(), &UIChooserView::sltMinimumWidthHintChanged);
    connect(model(), &UIChooserModel::sigToolMenuRequested,
            this, &UIChooser::sltToolMenuRequested);
    connect(model(), &UIChooserModel::sigCloudMachineStateChange,
            this, &UIChooser::sigCloudMachineStateChange);
    connect(model(), &UIChooserModel::sigStartOrShowRequest,
            this, &UIChooser::sigStartOrShowRequest);

    /* Chooser-view connections: */
    connect(view(), &UIChooserView::sigResized,
            model(), &UIChooserModel::sltHandleViewResized);
    connect(view(), &UIChooserView::sigSearchWidgetVisibilityChanged,
            this, &UIChooser::sigMachineSearchWidgetVisibilityChanged);
}

void UIChooser::initModel()
{
    AssertPtrReturnVoid(model());
    model()->init();
}

void UIChooser::deinitModel()
{
    AssertPtrReturnVoid(model());
    model()->deinit();
}

void UIChooser::cleanupConnections()
{
    AssertPtrReturnVoid(model());
    AssertPtrReturnVoid(view());

    /* Chooser-model connections: */
    disconnect(model(), &UIChooserModel::sigGroupSavingStateChanged,
               this, &UIChooser::sigGroupSavingStateChanged);
    disconnect(model(), &UIChooserModel::sigSelectionChanged,
               this, &UIChooser::sigSelectionChanged);
    disconnect(model(), &UIChooserModel::sigSelectionInvalidated,
               this, &UIChooser::sigSelectionInvalidated);
    disconnect(model(), &UIChooserModel::sigToggleStarted,
               this, &UIChooser::sigToggleStarted);
    disconnect(model(), &UIChooserModel::sigToggleFinished,
               this, &UIChooser::sigToggleFinished);
    disconnect(model(), &UIChooserModel::sigRootItemMinimumWidthHintChanged,
               view(), &UIChooserView::sltMinimumWidthHintChanged);
    disconnect(model(), &UIChooserModel::sigToolMenuRequested,
               this, &UIChooser::sltToolMenuRequested);
    disconnect(model(), &UIChooserModel::sigCloudMachineStateChange,
               this, &UIChooser::sigCloudMachineStateChange);
    disconnect(model(), &UIChooserModel::sigStartOrShowRequest,
               this, &UIChooser::sigStartOrShowRequest);

    /* Chooser-view connections: */
    disconnect(view(), &UIChooserView::sigResized,
               model(), &UIChooserModel::sltHandleViewResized);
    disconnect(view(), &UIChooserView::sigSearchWidgetVisibilityChanged,
               this, &UIChooser::sigMachineSearchWidgetVisibilityChanged);
}

void UIChooser::cleanup()
{
    /* Deinit model: */
    deinitModel();

    /* Cleanup everything: */
    cleanupConnections();
}
