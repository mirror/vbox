/* $Id$ */
/** @file
 * VBox Qt GUI - UIGInformationElement class implementation.
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
# include <QGraphicsView>
# include <QStateMachine>
# include <QPropertyAnimation>
# include <QSignalTransition>
# include <QStyleOptionGraphicsItem>
# include <QGraphicsSceneMouseEvent>

/* GUI includes: */
# include "UIGInformationElement.h"
# include "UIGInformationSet.h"
# include "UIGInformationModel.h"
# include "UIGraphicsRotatorButton.h"
# include "UIGraphicsTextPane.h"
# include "UIActionPool.h"
# include "UIIconPool.h"
# include "UIConverter.h"
# include "VBoxGlobal.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIGInformationElement::UIGInformationElement(UIGInformationSet *pParent, InformationElementType type, bool fOpened)
    : UIGInformationItem(pParent)
    , m_pSet(pParent)
    , m_type(type)
    , m_iCornerRadius(0)
    , m_iMinimumHeaderWidth(0)
    , m_iMinimumHeaderHeight(0)
    , m_pTextPane(0)
{
    /* Prepare element: */
    prepareElement();
    /* Prepare text-pane: */
    prepareTextPane();
    /* Setup size-policy: */
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    /* Add item to the parent: */
    AssertMsg(parentItem(), ("No parent set for details element!"));
    parentItem()->addItem(this);
}

UIGInformationElement::~UIGInformationElement()
{
    /* Remove item from the parent: */
    AssertMsg(parentItem(), ("No parent set for details element!"));
    parentItem()->removeItem(this);
}

void UIGInformationElement::updateAppearance()
{
    /* Update anchor role restrictions: */
    ConfigurationAccessLevel cal = m_pSet->configurationAccessLevel();
    m_pTextPane->setAnchorRoleRestricted("#mount", cal == ConfigurationAccessLevel_Null);
    m_pTextPane->setAnchorRoleRestricted("#attach", cal != ConfigurationAccessLevel_Full);
}

void UIGInformationElement::resizeEvent(QGraphicsSceneResizeEvent*)
{
    /* Update layout: */
    updateLayout();
}

QVariant UIGInformationElement::data(int iKey) const
{
    /* Provide other members with required data: */
    switch (iKey)
    {
        /* Hints: */
        case ElementData_Margin: return 5;
        case ElementData_Spacing: return 10;
        case ElementData_MinimumTextColumnWidth: return 100;
        /* Default: */
        default: break;
    }
    return QVariant();
}

void UIGInformationElement::updateMinimumHeaderWidth()
{
    /* Prepare variables: */
    int iSpacing = data(ElementData_Spacing).toInt();

    /* Update minimum-header-width: */
    m_iMinimumHeaderWidth = m_pixmapSize.width() +
                            iSpacing + m_nameSize.width();
}

void UIGInformationElement::updateMinimumHeaderHeight()
{
    /* Update minimum-header-height: */
    m_iMinimumHeaderHeight = qMax(m_pixmapSize.height(), m_nameSize.height());
}

void UIGInformationElement::setIcon(const QIcon &icon)
{
    /* Cache icon: */
    if (icon.isNull())
    {
        /* No icon provided: */
        m_pixmapSize = QSize();
        m_pixmap = QPixmap();
    }
    else
    {
        /* Determine default the icon size: */
        const QStyle *pStyle = QApplication::style();
        const int iIconMetric = pStyle->pixelMetric(QStyle::PM_SmallIconSize);
        m_pixmapSize = QSize(iIconMetric, iIconMetric);
        /* Acquire the icon of the corresponding size: */
        m_pixmap = icon.pixmap(m_pixmapSize);
    }

    /* Update linked values: */
    updateMinimumHeaderWidth();
    updateMinimumHeaderHeight();
}

void UIGInformationElement::setName(const QString &strName)
{
    /* Cache name: */
    m_strName = strName;
    QFontMetrics fm(m_nameFont, model()->paintDevice());
    m_nameSize = QSize(fm.width(m_strName), fm.height());

    /* Update linked values: */
    updateMinimumHeaderWidth();
    updateMinimumHeaderHeight();
}

const UITextTable& UIGInformationElement::text() const
{
    /* Retrieve text from text-pane: */
    return m_pTextPane->text();
}

void UIGInformationElement::setText(const UITextTable &text)
{
    /* Pass text to text-pane: */
    m_pTextPane->setText(text);
}

const CMachine& UIGInformationElement::machine()
{
    return m_pSet->machine();
}

int UIGInformationElement::minimumWidthHint() const
{
    /* Prepare variables: */
    int iMargin = data(ElementData_Margin).toInt();
    int iMinimumWidthHint = 0;

    /* Maximum width: */
    iMinimumWidthHint = qMax(m_iMinimumHeaderWidth, (int)m_pTextPane->minimumSizeHint().width());

    /* And 4 margins: 2 left and 2 right: */
    iMinimumWidthHint += 4 * iMargin;

    /* Return result: */
    return iMinimumWidthHint;
}

int UIGInformationElement::minimumHeightHint(bool fClosed) const
{
    /* Prepare variables: */
    int iMargin = data(ElementData_Margin).toInt();
    int iMinimumHeightHint = 0;

    /* Two margins: */
    iMinimumHeightHint += 2 * iMargin;

    /* Header height: */
    iMinimumHeightHint += m_iMinimumHeaderHeight + m_pTextPane->minimumSizeHint().height() + 10;

    /* Return value: */
    return iMinimumHeightHint;
}

int UIGInformationElement::minimumHeightHint() const
{
    return minimumHeightHint(false);
}

void UIGInformationElement::updateLayout()
{
    /* Prepare variables: */
    QSize size = geometry().size().toSize();
    int iMargin = data(ElementData_Margin).toInt();
    /* Layout text-pane: */
    int iTextPaneX = 2 * iMargin;
    int iTextPaneY = iMargin + m_iMinimumHeaderHeight + 2 * iMargin;
    m_pTextPane->setPos(iTextPaneX, iTextPaneY);
    m_pTextPane->resize(size.width() - 4 * iMargin,
                        size.height() - 4 * iMargin - m_iMinimumHeaderHeight);
    /* Show text-pane if still invisible and animation finished: */
    if (!m_pTextPane->isVisible())
        m_pTextPane->show();
}

void UIGInformationElement::addItem(UIGInformationItem*)
{
    AssertMsgFailed(("Details element do NOT support children!"));
}

void UIGInformationElement::removeItem(UIGInformationItem*)
{
    AssertMsgFailed(("Details element do NOT support children!"));
}

QList<UIGInformationItem*> UIGInformationElement::items(UIGInformationItemType) const
{
    AssertMsgFailed(("Details element do NOT support children!"));
    return QList<UIGInformationItem*>();
}

bool UIGInformationElement::hasItems(UIGInformationItemType) const
{
    AssertMsgFailed(("Details element do NOT support children!"));
    return false;
}

void UIGInformationElement::clearItems(UIGInformationItemType)
{
    AssertMsgFailed(("Details element do NOT support children!"));
}

void UIGInformationElement::prepareElement()
{
    /* Initialization: */
    m_nameFont = font();
    m_nameFont.setWeight(QFont::Bold);
    m_textFont = font();
}

void UIGInformationElement::prepareTextPane()
{
    /* Create text-pane: */
    m_pTextPane = new UIGraphicsTextPane(this, model()->paintDevice());
    /* Make sure item text is selectable: */
    m_pTextPane->setFlag(QGraphicsItem::ItemIsSelectable);
    connect(m_pTextPane, SIGNAL(sigGeometryChanged()), this, SLOT(sltUpdateGeometry()));
}

void UIGInformationElement::paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, QWidget*)
{
    /* Paint decorations: */
    paintDecorations(pPainter, pOption);

    /* Paint element info: */
    paintElementInfo(pPainter, pOption);
}

void UIGInformationElement::paintDecorations(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption)
{
    /* Paint background: */
    paintBackground(pPainter, pOption);
}

void UIGInformationElement::paintElementInfo(QPainter *pPainter, const QStyleOptionGraphicsItem*)
{
    /* Initialize some necessary variables: */
    int iMargin = data(ElementData_Margin).toInt();
    int iSpacing = data(ElementData_Spacing).toInt();

    /* Calculate attributes: */
    int iPixmapHeight = m_pixmapSize.height();
    int iNameHeight = m_nameSize.height();
    int iMaximumHeight = qMax(iPixmapHeight, iNameHeight);

    /* Prepare color: */
    QPalette pal = palette();
    QColor buttonTextColor = pal.color(QPalette::Active, QPalette::ButtonText);

    /* Paint pixmap: */
    int iElementPixmapX = 2 * iMargin;
    int iElementPixmapY = iPixmapHeight == iMaximumHeight ?
                          iMargin : iMargin + (iMaximumHeight - iPixmapHeight) / 2;
    paintPixmap(/* Painter: */
                pPainter,
                /* Rectangle to paint in: */
                QRect(QPoint(iElementPixmapX, iElementPixmapY), m_pixmapSize),
                /* Pixmap to paint: */
                m_pixmap);

    /* Paint name: */
    int iMachineNameX = iElementPixmapX +
                        m_pixmapSize.width() +
                        iSpacing;
    int iMachineNameY = iNameHeight == iMaximumHeight ?
                        iMargin : iMargin + (iMaximumHeight - iNameHeight) / 2;
    paintText(/* Painter: */
              pPainter,
              /* Rectangle to paint in: */
              QPoint(iMachineNameX, iMachineNameY),
              /* Font to paint text: */
              m_nameFont,
              /* Paint device: */
              model()->paintDevice(),
              /* Text to paint: */
              m_strName,
              /* buttonTextColor: */
              buttonTextColor);
}

void UIGInformationElement::paintBackground(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption)
{
    /* Save painter: */
    pPainter->save();

    /* Prepare variables: */
    int iMargin = data(ElementData_Margin).toInt();
    int iHeaderHeight = 2 * iMargin + m_iMinimumHeaderHeight;
    QRect optionRect = pOption->rect;
    QRect fullRect = optionRect;
    int iFullHeight = fullRect.height();

    /* Prepare color: */
    QPalette pal = palette();
    QColor headerColor = pal.color(QPalette::Active, QPalette::Button);
    QColor strokeColor = pal.color(QPalette::Active, QPalette::Mid);
    QColor bodyColor = pal.color(QPalette::Active, QPalette::Base);

    /* Add clipping: */
    QPainterPath path;
    path.moveTo(m_iCornerRadius, 0);
    path.arcTo(QRectF(path.currentPosition(), QSizeF(2 * m_iCornerRadius, 2 * m_iCornerRadius)).translated(-m_iCornerRadius, 0), 90, 90);
    path.lineTo(path.currentPosition().x(), iFullHeight - m_iCornerRadius);
    path.arcTo(QRectF(path.currentPosition(), QSizeF(2 * m_iCornerRadius, 2 * m_iCornerRadius)).translated(0, -m_iCornerRadius), 180, 90);
    path.lineTo(fullRect.width() - m_iCornerRadius, path.currentPosition().y());
    path.arcTo(QRectF(path.currentPosition(), QSizeF(2 * m_iCornerRadius, 2 * m_iCornerRadius)).translated(-m_iCornerRadius, -2 * m_iCornerRadius), 270, 90);
    path.lineTo(path.currentPosition().x(), m_iCornerRadius);
    path.arcTo(QRectF(path.currentPosition(), QSizeF(2 * m_iCornerRadius, 2 * m_iCornerRadius)).translated(-2 * m_iCornerRadius, -m_iCornerRadius), 0, 90);
    path.closeSubpath();
    pPainter->setClipPath(path);

    /* Calculate top rectangle: */
    QRect tRect = fullRect;
    tRect.setBottom(tRect.top() + iHeaderHeight);
    /* Calculate bottom rectangle: */
    QRect bRect = fullRect;
    bRect.setTop(tRect.bottom());

    /* Prepare top gradient: */
    QLinearGradient tGradient(tRect.bottomLeft(), tRect.topLeft());
    tGradient.setColorAt(0, headerColor.darker(110));

    /* Paint all the stuff: */
    pPainter->fillRect(tRect, tGradient);
    pPainter->fillRect(bRect, bodyColor);

    /* Stroke path: */
    pPainter->setClipping(false);
    pPainter->strokePath(path, strokeColor);

    /* Restore painter: */
    pPainter->restore();
}

