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
    return scene()->itemAt(position, QTransform());
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

