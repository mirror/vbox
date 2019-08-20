/* $Id$ */
/** @file
 * VBox Qt GUI - UIInformationWidget class implementation.
 */

/*
 * Copyright (C) 2016-2019 Oracle Corporation
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
#include <QHeaderView>
#include <QTableWidget>
#include <QTextDocument>
#include <QVBoxLayout>

/* GUI includes: */
#include "UIInformationWidget.h"
#include "UIInformationItem.h"


UIInformationWidget::UIInformationWidget(QWidget *pParent, const CMachine &machine, const CConsole &console)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_machine(machine)
    , m_console(console)
    , m_pMainLayout(0)
    , m_pTableWidget(0)
    , m_iColumCount(3)
    , m_iRowLeftMargin(0.2 * qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin))
    , m_iRowTopMargin(0.2 * qApp->style()->pixelMetric(QStyle::PM_LayoutTopMargin))
    , m_iRowRightMargin(0.2 * qApp->style()->pixelMetric(QStyle::PM_LayoutRightMargin))
    , m_iRowBottomMargin(0.2 * qApp->style()->pixelMetric(QStyle::PM_LayoutBottomMargin))
{
    prepareObjects();
}

void UIInformationWidget::prepareObjects()
{
    /* Create layout: */
    m_pMainLayout = new QVBoxLayout(this);
    if (!m_pMainLayout)
        return;
    m_pMainLayout->setSpacing(0);

    m_pTableWidget = new QTableWidget;
    if (m_pTableWidget)
    {
        /* Configure the table by hiding the headers etc.: */
        m_pTableWidget->setColumnCount(m_iColumCount);
        m_pTableWidget->verticalHeader()->hide();
        m_pTableWidget->horizontalHeader()->hide();
        m_pTableWidget->setShowGrid(false);
        m_pTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_pTableWidget->setFocusPolicy(Qt::NoFocus);
        m_pTableWidget->setSelectionMode(QAbstractItemView::NoSelection);
        m_pMainLayout->addWidget(m_pTableWidget);
    }
}



void UIInformationWidget::insertInfoRows(const UITextTable &table, const QFontMetrics &fontMetrics,
                                                QTextDocument &textDocument, int &iMaxColumn1Length)
{
    foreach (const UITextTableLine &line, table)
    {
        textDocument.setHtml(line.string2());
        insertInfoRow(line.string1(), textDocument.toPlainText(), fontMetrics, iMaxColumn1Length);
    }
}

void UIInformationWidget::insertTitleRow(const QString &strTitle, const QIcon &icon, const QFontMetrics &fontMetrics)
{
    int iRow = m_pTableWidget->rowCount();
    m_pTableWidget->insertRow(iRow);
    QSize iconSize;
    icon.actualSize(iconSize);
    m_pTableWidget->setRowHeight(iRow,
                                 qMax(fontMetrics.height() + m_iRowTopMargin + m_iRowBottomMargin, iconSize.height()));
    m_pTableWidget->setItem(iRow, 0, new QTableWidgetItem(icon, ""));
    QTableWidgetItem *pTitleItem = new QTableWidgetItem(strTitle);
    QFont font = pTitleItem->font();
    font.setBold(true);
    pTitleItem->setFont(font);
    m_pTableWidget->setItem(iRow, 1, pTitleItem);
}

void UIInformationWidget::insertInfoRow(const QString strText1, const QString &strText2,
                                               const QFontMetrics &fontMetrics, int &iMaxColumn1Length)
{
    int iRow = m_pTableWidget->rowCount();
    m_pTableWidget->insertRow(iRow);
    m_pTableWidget->setRowHeight(iRow, fontMetrics.height() + m_iRowTopMargin + m_iRowBottomMargin);
    iMaxColumn1Length = qMax(iMaxColumn1Length, fontMetrics.width(strText1));
    m_pTableWidget->setItem(iRow, 1, new QTableWidgetItem(strText1));
    m_pTableWidget->setItem(iRow, 2, new QTableWidgetItem(strText2));
}

void UIInformationWidget::resetTable()
{
    if (m_pTableWidget)
    {
        m_pTableWidget->clear();
        m_pTableWidget->setRowCount(0);
        m_pTableWidget->setColumnCount(m_iColumCount);
    }
}
