/* $Id$ */
/** @file
 * VBox Qt GUI - UIMediumDetailsWidget class implementation.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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
# include <QLabel>
# include <QStackedLayout>
# include <QVBoxLayout>

/* GUI includes: */
# include "QIDialogButtonBox.h"
# include "QILabel.h"
# include "QITabWidget.h"
# include "UIConverter.h"
# include "UIIconPool.h"
# include "UIMediumDetailsWidget.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIMediumDetailsWidget::UIMediumDetailsWidget(EmbedTo enmEmbedding, QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_enmEmbedding(enmEmbedding)
    , m_oldData(UIDataMedium())
    , m_newData(UIDataMedium())
    , m_pFrame(0)
    , m_pLayoutDetails(0)
{
    /* Prepare: */
    prepare();
}

void UIMediumDetailsWidget::setCurrentType(UIMediumType enmType)
{
    /* If known type was requested => raise corresponding container: */
    if (m_aContainers.contains(enmType))
        m_pLayoutDetails->setCurrentWidget(infoContainer(enmType));
}

void UIMediumDetailsWidget::setData(const UIDataMedium &data)
{
    /* Cache old/new data: */
    m_oldData = data;
    m_newData = m_oldData;

    /* Load data: */
    loadData();
}

void UIMediumDetailsWidget::retranslateUi()
{
    /* Nothing for now. */
}

void UIMediumDetailsWidget::prepare()
{
    /* Prepare this: */
    prepareThis();

    /* Apply language settings: */
    retranslateUi();
}

void UIMediumDetailsWidget::prepareThis()
{
    /* Create layout: */
    QVBoxLayout *pLayout = new QVBoxLayout(this);
    AssertPtrReturnVoid(pLayout);
    {
        /* Configure layout: */
        pLayout->setContentsMargins(0, 0, 0, 0);

        /* Prepare frame: */
        prepareFrame();
    }
}

void UIMediumDetailsWidget::prepareFrame()
{
    /* Create frame: */
    m_pFrame = new QFrame;
    AssertPtrReturnVoid(m_pFrame);
    {
        /* Configure frame: */
        m_pFrame->setFrameShape(QFrame::Box);
        m_pFrame->setFrameShadow(QFrame::Sunken);
        m_pFrame->setAutoFillBackground(true);
        QPalette pal = m_pFrame->palette();
        pal.setColor(QPalette::Active, QPalette::Window, pal.color(QPalette::Active, QPalette::Base));
        m_pFrame->setPalette(pal);

        /* Create layout: */
        QVBoxLayout *pLayout = new QVBoxLayout(m_pFrame);
        AssertPtrReturnVoid(pLayout);
        {
            /* Configure layout: */
            pLayout->setContentsMargins(0, 0, 0, 0);

            /* Prepare 'Details' tab: */
            prepareTabDetails();
        }

        /* Add into layout: */
        layout()->addWidget(m_pFrame);
    }
}

void UIMediumDetailsWidget::prepareTabDetails()
{
    /* Create 'Details' tab: */
    QWidget *pTabDetails = new QWidget;
    AssertPtrReturnVoid(pTabDetails);
    {
        /* Create stacked layout: */
        m_pLayoutDetails = new QStackedLayout(pTabDetails);
        AssertPtrReturnVoid(m_pLayoutDetails);
        {
            /* Create information-containers: */
            for (int i = (int)UIMediumType_HardDisk; i < (int)UIMediumType_All; ++i)
            {
                const UIMediumType enmType = (UIMediumType)i;
                prepareInformationContainer(enmType, enmType == UIMediumType_HardDisk ? 7 : 3); /// @todo Remove hard-coded values.
            }
        }

        /* Add to tab-widget: */
        m_pFrame->layout()->addWidget(pTabDetails);
    }
}

void UIMediumDetailsWidget::prepareInformationContainer(UIMediumType enmType, int cFields)
{
    /* Create information-container: */
    m_aContainers[enmType] = new QWidget;
    QWidget *pContainer = infoContainer(enmType);
    AssertPtrReturnVoid(pContainer);
    {
        /* Create layout: */
        new QGridLayout(pContainer);
        QGridLayout *pLayout = qobject_cast<QGridLayout*>(pContainer->layout());
        AssertPtrReturnVoid(pLayout);
        {
            /* Configure layout: */
            pLayout->setVerticalSpacing(0);
            pLayout->setContentsMargins(5, 5, 5, 5);
            pLayout->setColumnStretch(1, 1);

            /* Create labels & fields: */
            int i = 0;
            for (; i < cFields; ++i)
            {
                /* Create label: */
                m_aLabels[enmType] << new QLabel;
                QLabel *pLabel = infoLabel(enmType, i);
                AssertPtrReturnVoid(pLabel);
                {
                    /* Add into layout: */
                    pLayout->addWidget(pLabel, i, 0);
                }

                /* Create field: */
                m_aFields[enmType] << new QILabel;
                QILabel *pField = infoField(enmType, i);
                AssertPtrReturnVoid(pField);
                {
                    /* Configure field: */
                    pField->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed));
                    pField->setFullSizeSelection(true);

                    /* Add into layout: */
                    pLayout->addWidget(pField, i, 1);
                }
            }

            /* Create stretch: */
            QSpacerItem *pSpacer = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
            AssertPtrReturnVoid(pSpacer);
            {
                /* Add into layout: */
                pLayout->addItem(pSpacer, i, 0, 1, 2);
            }
        }

        /* Add into layout: */
        m_pLayoutDetails->addWidget(pContainer);
    }
}

void UIMediumDetailsWidget::loadData()
{
    /* Load details: */
    loadDetails();
}

void UIMediumDetailsWidget::loadDetails()
{
    /* Get information-labels just to acquire their number: */
    const QList<QLabel*> aLabels = m_aLabels.value(m_newData.m_enmType, QList<QLabel*>());
    /* Get information-fields just to acquire their number: */
    const QList<QILabel*> aFields = m_aFields.value(m_newData.m_enmType, QList<QILabel*>());
    /* For each the label => update contents: */
    for (int i = 0; i < aLabels.size(); ++i)
        infoLabel(m_newData.m_enmType, i)->setText(m_newData.m_details.m_aLabels.value(i, QString()));
    /* For each the field => update contents: */
    for (int i = 0; i < aFields.size(); ++i)
    {
        infoField(m_newData.m_enmType, i)->setText(m_newData.m_details.m_aFields.value(i, QString()));
        infoField(m_newData.m_enmType, i)->setEnabled(!infoField(m_newData.m_enmType, i)->text().trimmed().isEmpty());
    }
}

QWidget *UIMediumDetailsWidget::infoContainer(UIMediumType enmType) const
{
    /* Return information-container for known medium type: */
    return m_aContainers.value(enmType, 0);
}

QLabel *UIMediumDetailsWidget::infoLabel(UIMediumType enmType, int iIndex) const
{
    /* Acquire list of labels: */
    const QList<QLabel*> aLabels = m_aLabels.value(enmType, QList<QLabel*>());

    /* Return label for known index: */
    return aLabels.value(iIndex, 0);
}

QILabel *UIMediumDetailsWidget::infoField(UIMediumType enmType, int iIndex) const
{
    /* Acquire list of fields: */
    const QList<QILabel*> aFields = m_aFields.value(enmType, QList<QILabel*>());

    /* Return label for known index: */
    return aFields.value(iIndex, 0);
}

