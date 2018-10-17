/* $Id$ */
/** @file
 * VBox Qt GUI - UICloudProfileDetailsWidget class implementation.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
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
# include <QHeaderView>
# include <QPushButton>
# include <QTableWidget>
# include <QVBoxLayout>

/* GUI includes: */
# include "QIDialogButtonBox.h"
# include "UICloudProfileDetailsWidget.h"

/* Other VBox includes: */
# include "iprt/assert.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UICloudProfileDetailsWidget::UICloudProfileDetailsWidget(EmbedTo enmEmbedding, QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_enmEmbedding(enmEmbedding)
    , m_pTableWidget(0)
    , m_pButtonBox(0)
{
    /* Prepare: */
    prepare();
}

void UICloudProfileDetailsWidget::setData(const UIDataCloudProfile &data)
{
    /* Cache old/new data: */
    m_oldData = data;
    m_newData = m_oldData;

    /* Load data: */
    loadData();
}

void UICloudProfileDetailsWidget::retranslateUi()
{
    /// @todo add description tool-tips

    /* Translate table-widget: */
    m_pTableWidget->setToolTip(tr("Contains cloud profile settings"));

    /* Retranslate validation: */
    retranslateValidation();

    /* Update table tool-tips: */
    updateTableToolTips();
}

void UICloudProfileDetailsWidget::sltTableChanged()
{
    /// @todo handle profile settings table change!
}

void UICloudProfileDetailsWidget::sltHandleButtonBoxClick(QAbstractButton *pButton)
{
    /* Make sure button-box exists: */
    AssertPtrReturnVoid(m_pButtonBox);

    /* Disable buttons first of all: */
    m_pButtonBox->button(QDialogButtonBox::Cancel)->setEnabled(false);
    m_pButtonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    /* Compare with known buttons: */
    if (pButton == m_pButtonBox->button(QDialogButtonBox::Cancel))
        emit sigDataChangeRejected();
    else
    if (pButton == m_pButtonBox->button(QDialogButtonBox::Ok))
        emit sigDataChangeAccepted();
}

void UICloudProfileDetailsWidget::prepare()
{
    /* Prepare widgets: */
    prepareWidgets();

    /* Apply language settings: */
    retranslateUi();

    /* Update button states finally: */
    updateButtonStates();
}

void UICloudProfileDetailsWidget::prepareWidgets()
{
    /* Create layout: */
    QVBoxLayout *pLayout = new QVBoxLayout(this);
    if (pLayout)
    {
        /// @todo sync layout margins with other tools!

        /* Create tab-widget: */
        m_pTableWidget = new QTableWidget;
        if (m_pTableWidget)
        {
            m_pTableWidget->setAlternatingRowColors(true);
            m_pTableWidget->horizontalHeader()->setVisible(false);
            m_pTableWidget->verticalHeader()->setVisible(false);
            m_pTableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

            /* Add into layout: */
            pLayout->addWidget(m_pTableWidget);
        }

        /* If parent embedded into stack: */
        if (m_enmEmbedding == EmbedTo_Stack)
        {
            /* Create button-box: */
            m_pButtonBox = new QIDialogButtonBox;
            AssertPtrReturnVoid(m_pButtonBox);
            /* Configure button-box: */
            m_pButtonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
            connect(m_pButtonBox, &QIDialogButtonBox::clicked, this, &UICloudProfileDetailsWidget::sltHandleButtonBoxClick);

            /* Add into layout: */
            pLayout->addWidget(m_pButtonBox);
        }
    }
}

void UICloudProfileDetailsWidget::loadData()
{
    /* Clear table initially: */
    m_pTableWidget->clear();

    /* Configure table: */
    m_pTableWidget->setRowCount(m_oldData.m_data.keys().size());
    m_pTableWidget->setColumnCount(2);

    /* Push acquired keys/values to data fields: */
    for (int i = 0; i < m_pTableWidget->rowCount(); ++i)
    {
        /* Gather values: */
        const QString strKey = m_oldData.m_data.keys().at(i);
        const QString strValue = m_oldData.m_data.value(strKey).first;
        const QString strToolTip = m_oldData.m_data.value(strKey).second;

        /* Create key item: */
        QTableWidgetItem *pItemK = new QTableWidgetItem(strKey);
        if (pItemK)
        {
            /* Non-editable for sure, but non-selectable? */
            pItemK->setFlags(pItemK->flags() & ~Qt::ItemIsEditable);
            /* Use non-translated description as tool-tip: */
            pItemK->setData(Qt::UserRole, strToolTip);

            /* Insert into table: */
            m_pTableWidget->setItem(i, 0, pItemK);
        }

        /* Create value item: */
        QTableWidgetItem *pItemV = new QTableWidgetItem(strValue);
        if (pItemV)
        {
            /* Use the value as tool-tip, there can be quite long values: */
            pItemV->setToolTip(strValue);

            /* Insert into table: */
            m_pTableWidget->setItem(i, 1, pItemV);
        }
    }

    /* Update table tooltips: */
    updateTableToolTips();
    /* Adjust table contents: */
    adjustTableContents();
}

void UICloudProfileDetailsWidget::revalidate(QWidget *pWidget /* = 0 */)
{
    /// @todo validate profile settings table!

    /* Retranslate validation: */
    retranslateValidation(pWidget);
}

void UICloudProfileDetailsWidget::retranslateValidation(QWidget *pWidget /* = 0 */)
{
    Q_UNUSED(pWidget);

    /// @todo retranslate profile settings vaidation!
}

void UICloudProfileDetailsWidget::updateTableToolTips()
{
    /* Iterate through all the key items: */
    for (int i = 0; i < m_pTableWidget->rowCount(); ++i)
    {
        /* Acquire current key item: */
        QTableWidgetItem *pItemK = m_pTableWidget->item(i, 0);
        if (pItemK)
        {
            const QString strToolTip = pItemK->data(Qt::UserRole).toString();
            pItemK->setToolTip(tr(strToolTip.toUtf8().constData()));
        }
    }
}

void UICloudProfileDetailsWidget::adjustTableContents()
{
    /* Disable last column stretching temporary: */
    m_pTableWidget->horizontalHeader()->setStretchLastSection(false);

    /* Resize both columns to contents: */
    m_pTableWidget->resizeColumnsToContents();
    /* Then acquire full available width: */
    const int iFullWidth = m_pTableWidget->viewport()->width();
    /* First column should not be less than it's minimum size, last gets the rest: */
    const int iMinimumWidth0 = qMin(m_pTableWidget->horizontalHeader()->sectionSize(0), iFullWidth / 2);
    m_pTableWidget->horizontalHeader()->resizeSection(0, iMinimumWidth0);

    /* Enable last column stretching again: */
    m_pTableWidget->horizontalHeader()->setStretchLastSection(true);
}

void UICloudProfileDetailsWidget::updateButtonStates()
{
    /// @todo update that printf as well!
#if 0
    if (m_oldData != m_newData)
        printf("Interface: %s, %s, %s, %s;  DHCP server: %d, %s, %s, %s, %s\n",
               m_newData.m_interface.m_strAddress.toUtf8().constData(),
               m_newData.m_interface.m_strMask.toUtf8().constData(),
               m_newData.m_interface.m_strAddress6.toUtf8().constData(),
               m_newData.m_interface.m_strPrefixLength6.toUtf8().constData(),
               (int)m_newData.m_dhcpserver.m_fEnabled,
               m_newData.m_dhcpserver.m_strAddress.toUtf8().constData(),
               m_newData.m_dhcpserver.m_strMask.toUtf8().constData(),
               m_newData.m_dhcpserver.m_strLowerAddress.toUtf8().constData(),
               m_newData.m_dhcpserver.m_strUpperAddress.toUtf8().constData());
#endif

    /* Update 'Apply' / 'Reset' button states: */
    if (m_pButtonBox)
    {
        m_pButtonBox->button(QDialogButtonBox::Cancel)->setEnabled(m_oldData != m_newData);
        m_pButtonBox->button(QDialogButtonBox::Ok)->setEnabled(m_oldData != m_newData);
    }

    /* Notify listeners as well: */
    emit sigDataChanged(m_oldData != m_newData);
}
