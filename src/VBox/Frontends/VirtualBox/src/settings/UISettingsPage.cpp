/* $Id$ */
/** @file
 * VBox Qt GUI - UISettingsPage class implementation.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/* Qt includes: */
#include <QLabel>
#include <QPainter>
#include <QStyle>
#include <QVariant>
#include <QVBoxLayout>

/* GUI includes: */
#include "UIConverter.h"
#include "UISettingsPage.h"
#include "UISettingsPageValidator.h"


/*********************************************************************************************************************************
*   Class UISettingsPage implementation.                                                                                         *
*********************************************************************************************************************************/

UISettingsPage::UISettingsPage()
    : m_enmConfigurationAccessLevel(ConfigurationAccessLevel_Null)
    , m_cId(-1)
    , m_pFirstWidget(0)
    , m_pValidator(0)
    , m_fIsValidatorBlocked(true)
    , m_fProcessed(false)
    , m_fFailed(false)
{
}

void UISettingsPage::notifyOperationProgressError(const QString &strErrorInfo)
{
    QMetaObject::invokeMethod(this,
                              "sigOperationProgressError",
                              Qt::BlockingQueuedConnection,
                              Q_ARG(QString, strErrorInfo));
}

void UISettingsPage::setValidator(UISettingsPageValidator *pValidator)
{
    /* Make sure validator is not yet assigned: */
    AssertMsg(!m_pValidator, ("Validator already assigned!\n"));
    if (m_pValidator)
        return;

    /* Assign validator: */
    m_pValidator = pValidator;
}

void UISettingsPage::setConfigurationAccessLevel(ConfigurationAccessLevel enmConfigurationAccessLevel)
{
    m_enmConfigurationAccessLevel = enmConfigurationAccessLevel;
    polishPage();
}

void UISettingsPage::filterOut(const QString &strFilter)
{
    /* Propagate filter towards all the children: */
    foreach (UIEditor *pEditor, m_editors)
        pEditor->filterOut(strFilter);

    /* Check if at least one of children visible: */
    bool fVisible = false;
    foreach (UIEditor *pEditor, m_editors)
        if (pEditor->isVisibleTo(this))
        {
            fVisible = true;
            break;
        }

    /* Update page visibility: */
    setVisible(fVisible);
}

void UISettingsPage::revalidate()
{
    /* Invalidate validator if allowed: */
    if (!m_fIsValidatorBlocked && m_pValidator)
        m_pValidator->invalidate();
}


/*********************************************************************************************************************************
*   Class UISettingsPageGlobal implementation.                                                                                   *
*********************************************************************************************************************************/

UISettingsPageGlobal::UISettingsPageGlobal()
{
}

GlobalSettingsPageType UISettingsPageGlobal::internalID() const
{
    return static_cast<GlobalSettingsPageType>(id());
}

QString UISettingsPageGlobal::internalName() const
{
    return gpConverter->toInternalString(internalID());
}

QPixmap UISettingsPageGlobal::warningPixmap() const
{
    return gpConverter->toWarningPixmap(internalID());
}

void UISettingsPageGlobal::fetchData(const QVariant &data)
{
    /* Fetch data to m_host & m_properties: */
    m_host = data.value<UISettingsDataGlobal>().m_host;
    m_properties = data.value<UISettingsDataGlobal>().m_properties;
}

void UISettingsPageGlobal::uploadData(QVariant &data) const
{
    /* Upload m_host & m_properties to data: */
    data = QVariant::fromValue(UISettingsDataGlobal(m_host, m_properties));
}


/*********************************************************************************************************************************
*   Class UISettingsPageMachine implementation.                                                                                  *
*********************************************************************************************************************************/

UISettingsPageMachine::UISettingsPageMachine()
{
}

MachineSettingsPageType UISettingsPageMachine::internalID() const
{
    return static_cast<MachineSettingsPageType>(id());
}

QString UISettingsPageMachine::internalName() const
{
    return gpConverter->toInternalString(internalID());
}

QPixmap UISettingsPageMachine::warningPixmap() const
{
    return gpConverter->toWarningPixmap(internalID());
}

void UISettingsPageMachine::fetchData(const QVariant &data)
{
    /* Fetch data to m_machine & m_console: */
    m_machine = data.value<UISettingsDataMachine>().m_machine;
    m_console = data.value<UISettingsDataMachine>().m_console;
}

void UISettingsPageMachine::uploadData(QVariant &data) const
{
    /* Upload m_machine & m_console to data: */
    data = QVariant::fromValue(UISettingsDataMachine(m_machine, m_console));
}


/*********************************************************************************************************************************
*   Class UISettingsPageFrame implementation.                                                                                    *
*********************************************************************************************************************************/

UISettingsPageFrame::UISettingsPageFrame(UISettingsPage *pPage, QWidget *pParent /* = 0 */)
    : UIEditor(pParent)
    , m_pPage(pPage)
    , m_pLabelName(0)
    , m_pWidget(0)
    , m_pLayout(0)
{
    prepare();
}

void UISettingsPageFrame::setName(const QString &strName)
{
    if (m_strName == strName)
        return;

    m_strName = strName;
    if (m_pLabelName)
        m_pLabelName->setText(m_strName);
}

void UISettingsPageFrame::filterOut(const QString &strFilter)
{
    /* Propagate filter to the child: */
    AssertReturnVoid(m_editors.size() == 1);
    m_editors.first()->filterOut(strFilter);

    /* Check if child visible: */
    const bool fVisible = m_editors.first()->isVisibleTo(this);

    /* Update frame visibility: */
    setVisible(fVisible);
}

void UISettingsPageFrame::retranslateUi()
{
    // No NLS tags for now; We are receiving our name through the getter.
}

void UISettingsPageFrame::paintEvent(QPaintEvent *pPaintEvent)
{
    /* Prepare painter: */
    QPainter painter(this);
    /* Avoid painting more than necessary: */
    painter.setClipRect(pPaintEvent->rect());

    /* Prepare palette colors: */
    const QPalette pal = QApplication::palette();
    QColor color0 = pal.color(QPalette::Window);
    QColor color1 = pal.color(QPalette::Window).lighter(110);
    color1.setAlpha(0);
    QColor color2 = pal.color(QPalette::Window).darker(200);

    /* Acquire contents rect: */
    QRect cRect = m_pWidget->geometry();
    const int iX = cRect.x();
    const int iY = cRect.y();
    const int iWidth = cRect.width();
    const int iHeight = cRect.height();

    /* Invent pixel metric: */
    const int iMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 4;

    /* Top-left corner: */
    QRadialGradient grad1(QPointF(iX + iMetric, iY + iMetric), iMetric);
    {
        grad1.setColorAt(0, color2);
        grad1.setColorAt(1, color1);
    }
    /* Top-right corner: */
    QRadialGradient grad2(QPointF(iX + iWidth - iMetric, iY + iMetric), iMetric);
    {
        grad2.setColorAt(0, color2);
        grad2.setColorAt(1, color1);
    }
    /* Bottom-left corner: */
    QRadialGradient grad3(QPointF(iX + iMetric, iY + iHeight - iMetric), iMetric);
    {
        grad3.setColorAt(0, color2);
        grad3.setColorAt(1, color1);
    }
    /* Botom-right corner: */
    QRadialGradient grad4(QPointF(iX + iWidth - iMetric, iY + iHeight - iMetric), iMetric);
    {
        grad4.setColorAt(0, color2);
        grad4.setColorAt(1, color1);
    }

    /* Top line: */
    QLinearGradient grad5(QPointF(iX + iMetric, iY), QPointF(iX + iMetric, iY + iMetric));
    {
        grad5.setColorAt(0, color1);
        grad5.setColorAt(1, color2);
    }
    /* Bottom line: */
    QLinearGradient grad6(QPointF(iX + iMetric, iY + iHeight), QPointF(iX + iMetric, iY + iHeight - iMetric));
    {
        grad6.setColorAt(0, color1);
        grad6.setColorAt(1, color2);
    }
    /* Left line: */
    QLinearGradient grad7(QPointF(iX, iY + iHeight - iMetric), QPointF(iX + iMetric, iY + iHeight - iMetric));
    {
        grad7.setColorAt(0, color1);
        grad7.setColorAt(1, color2);
    }
    /* Right line: */
    QLinearGradient grad8(QPointF(iX + iWidth, iY + iHeight - iMetric), QPointF(iX + iWidth - iMetric, iY + iHeight - iMetric));
    {
        grad8.setColorAt(0, color1);
        grad8.setColorAt(1, color2);
    }

    /* Paint shape/shadow: */
    painter.fillRect(QRect(iX + iMetric,          iY + iMetric,           iWidth - iMetric * 2, iHeight - iMetric * 2), color0);
    painter.fillRect(QRect(iX,                    iY,                     iMetric,              iMetric),               grad1);
    painter.fillRect(QRect(iX + iWidth - iMetric, iY,                     iMetric,              iMetric),               grad2);
    painter.fillRect(QRect(iX,                    iY + iHeight - iMetric, iMetric,              iMetric),               grad3);
    painter.fillRect(QRect(iX + iWidth - iMetric, iY + iHeight - iMetric, iMetric,              iMetric),               grad4);
    painter.fillRect(QRect(iX + iMetric,          iY,                     iWidth - iMetric * 2, iMetric),               grad5);
    painter.fillRect(QRect(iX + iMetric,          iY + iHeight - iMetric, iWidth - iMetric * 2, iMetric),               grad6);
    painter.fillRect(QRect(iX,                    iY + iMetric,           iMetric,              iHeight - iMetric * 2), grad7);
    painter.fillRect(QRect(iX + iWidth - iMetric, iY + iMetric,           iMetric,              iHeight - iMetric * 2), grad8);
}

void UISettingsPageFrame::prepare()
{
    /* Keep minimum vertical size: */
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

    /* Create main layout: */
    QVBoxLayout *pLayoutMain = new QVBoxLayout(this);
    if (pLayoutMain)
    {
        pLayoutMain->setContentsMargins(0, 0, 0, 0);

        /* Create name label: */
        m_pLabelName = new QLabel(this);
        if (m_pLabelName)
            pLayoutMain->addWidget(m_pLabelName);

        /* Create contents widget: */
        m_pWidget = new QWidget(this);
        if (m_pWidget)
        {
            m_pLayout = new QVBoxLayout(m_pWidget);
            if (m_pLayout)
            {
                m_pLayout->addWidget(m_pPage);
                /// @todo what about removal handling?
                m_editors << m_pPage;
            }
            pLayoutMain->addWidget(m_pWidget);
        }
    }
}
