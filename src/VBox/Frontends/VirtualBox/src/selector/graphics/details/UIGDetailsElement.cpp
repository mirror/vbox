/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGDetailsElement class implementation
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
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QTextLayout>
#include <QGraphicsSceneMouseEvent>

/* GUI includes: */
#include "UIGDetailsElement.h"
#include "UIGDetailsSet.h"
#include "UIGDetailsModel.h"
#include "UIGraphicsRotatorButton.h"
#include "VBoxGlobal.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIIconPool.h"
#include "UIConverter.h"

UIGDetailsElement::UIGDetailsElement(UIGDetailsSet *pParent, DetailsElementType type, bool fOpened)
    : UIGDetailsItem(pParent)
    , m_pSet(pParent)
    , m_type(type)
    , m_fClosed(!fOpened)
    , m_pButton(0)
    , m_iAdditionalHeight(0)
    , m_iCornerRadius(6)
    , m_fHovered(false)
    , m_fNameHoveringAccessible(false)
    , m_fNameHovered(false)
{
    /* Prepare element: */
    prepareElement();
    /* Prepare button: */
    prepareButton();

    /* Update size-policy/hint: */
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    /* Update: */
    updateHoverAccessibility();

    /* Add item to the parent: */
    parentItem()->addItem(this);
}

UIGDetailsElement::~UIGDetailsElement()
{
    /* Remove item from the parent: */
    AssertMsg(parentItem(), ("No parent set for details element!"));
    parentItem()->removeItem(this);
}

DetailsElementType UIGDetailsElement::elementType() const
{
    return m_type;
}

bool UIGDetailsElement::closed() const
{
    return m_fClosed;
}

bool UIGDetailsElement::opened() const
{
    return !m_fClosed;
}

void UIGDetailsElement::close()
{
    m_pButton->setToggled(false, false);
}

void UIGDetailsElement::open()
{
    m_pButton->setToggled(true, false);
}

int UIGDetailsElement::minimumWidthHint() const
{
    /* First of all, we have to prepare few variables: */
    int iMargin = data(ElementData_Margin).toInt();
    int iHeaderWidth = data(ElementData_HeaderSize).toSize().width();
    int iTextWidth = data(ElementData_TextSize).toSize().width();

    /* Calculating proposed width: */
    int iProposedWidth = 0;

    /* Maximum width: */
    iProposedWidth = qMax(iHeaderWidth, iTextWidth);

    /* And 4 margins: 2 left and 2 right: */
    iProposedWidth += 4 * iMargin;

    /* Return result: */
    return iProposedWidth;
}

int UIGDetailsElement::minimumHeightHint() const
{
    return minimumHeightHint(m_fClosed);
}

void UIGDetailsElement::sltElementToggleStart()
{
    /* Setup animation: */
    updateAnimationParameters();

    /* Element closed, we are opening it: */
    if (m_fClosed)
    {
        /* Toggle-state will be updated
         * on toggle finish signal! */
    }
    /* Group opened, we are closing it: */
    else
    {
        /* Update toggle-state: */
        m_fClosed = true;
    }
}

void UIGDetailsElement::sltElementToggleFinish(bool fToggled)
{
    /* Update toggle-state: */
    m_fClosed = !fToggled;
    /* Relayout model: */
    model()->updateLayout();
    update();
}

void UIGDetailsElement::sltMachineStateChange(QString strId)
{
    /* Is this our VM changed? */
    if (machine().GetId() == strId)
        updateHoverAccessibility();

    /* Finally, update appearance: */
    sltShouldWeUpdateAppearance(strId);
}

void UIGDetailsElement::sltShouldWeUpdateAppearance(QString strId)
{
    if (machine().GetId() == strId)
        sltUpdateAppearance();
}

QVariant UIGDetailsElement::data(int iKey) const
{
    /* Provide other members with required data: */
    switch (iKey)
    {
        /* Layout hints: */
        case ElementData_Margin: return 5;
        case ElementData_Spacing: return 10;
        /* Pixmaps: */
        case ElementData_Pixmap: return m_icon;
        case ElementData_ButtonPixmap: return m_buttonIcon;
        /* Fonts: */
        case ElementData_NameFont:
        {
            QFont nameFont = qApp->font();
            nameFont.setWeight(QFont::Bold);
            return nameFont;
        }
        case ElementData_TextFont:
        {
            QFont textFont = qApp->font();
            return textFont;
        }
        /* Sizes: */
        case ElementData_PixmapSize:
            return m_icon.isNull() ? QSize(0, 0) : m_icon.availableSizes().at(0);
        case ElementData_NameSize:
        {
            QFontMetrics fm(data(ElementData_NameFont).value<QFont>());
            return QSize(fm.width(m_strName), fm.height());
        }
        case ElementData_ButtonSize: return m_pButton->minimumSizeHint();
        case ElementData_HeaderSize:
        {
            /* Prepare variables: */
            int iMargin = data(ElementData_Margin).toInt();
            int iSpacing = data(ElementData_Spacing).toInt();
            QSize pixmapSize = data(ElementData_PixmapSize).toSize();
            QSize nameSize = data(ElementData_NameSize).toSize();
            QSize buttonSize = data(ElementData_ButtonSize).toSize();
            /* Header width: */
            int iHeaderWidth = iMargin + pixmapSize.width() + iSpacing +
                               nameSize.width() + iSpacing + buttonSize.width();
            /* Header height: */
            int iHeaderHeight = qMax(pixmapSize.height(), nameSize.height());
            iHeaderHeight = qMax(iHeaderHeight, buttonSize.height());
            /* Return value: */
            return QSize(iHeaderWidth, iHeaderHeight);
        }
        case ElementData_TextSize:
        {
            int iSpacing = data(ElementData_Spacing).toInt();
            QFontMetrics fm(data(ElementData_TextFont).value<QFont>());
            int iLongestFirst = 0;
            int iLongestSecond = 0;
            foreach (const UITextTableLine &line, m_text)
            {
                iLongestFirst = qMax(iLongestFirst, fm.width(line.first));
                iLongestSecond = qMax(iLongestSecond, fm.width(line.second));
            }
            int iLongestLine = iLongestFirst + iSpacing + iLongestSecond;
            int iSummaryHeight = fm.height() * m_text.size();
            return QSize(iLongestLine, iSummaryHeight);
        }
        /* Default: */
        default: break;
    }
    return QVariant();
}

void UIGDetailsElement::setIcon(const QIcon &icon)
{
    m_icon = icon;
}

void UIGDetailsElement::setName(const QString &strName)
{
    m_strName = strName;
}

void UIGDetailsElement::setText(const UITextTable &text)
{
    m_text = text;
}

const CMachine& UIGDetailsElement::machine()
{
    return m_pSet->machine();
}

void UIGDetailsElement::addItem(UIGDetailsItem*)
{
    AssertMsgFailed(("Details element do NOT support children!"));
}

void UIGDetailsElement::removeItem(UIGDetailsItem*)
{
    AssertMsgFailed(("Details element do NOT support children!"));
}

QList<UIGDetailsItem*> UIGDetailsElement::items(UIGDetailsItemType) const
{
    AssertMsgFailed(("Details element do NOT support children!"));
    return QList<UIGDetailsItem*>();
}

bool UIGDetailsElement::hasItems(UIGDetailsItemType) const
{
    AssertMsgFailed(("Details element do NOT support children!"));
    return false;
}

void UIGDetailsElement::clearItems(UIGDetailsItemType)
{
    AssertMsgFailed(("Details element do NOT support children!"));
}

void UIGDetailsElement::prepareElement()
{
    connect(gVBoxEvents, SIGNAL(sigMachineStateChange(QString, KMachineState)), this, SLOT(sltMachineStateChange(QString)));
    connect(gVBoxEvents, SIGNAL(sigMachineDataChange(QString)), this, SLOT(sltShouldWeUpdateAppearance(QString)));
    connect(gVBoxEvents, SIGNAL(sigSessionStateChange(QString, KSessionState)), this, SLOT(sltShouldWeUpdateAppearance(QString)));
    connect(gVBoxEvents, SIGNAL(sigSnapshotChange(QString, QString)), this, SLOT(sltShouldWeUpdateAppearance(QString)));
    connect(&vboxGlobal(), SIGNAL(mediumEnumStarted()), this, SLOT(sltUpdateAppearance()));
    connect(&vboxGlobal(), SIGNAL(mediumEnumFinished(const VBoxMediaList &)), this, SLOT(sltUpdateAppearance()));
    connect(this, SIGNAL(sigToggleElement(DetailsElementType, bool)), model(), SLOT(sltToggleElements(DetailsElementType, bool)));
    connect(this, SIGNAL(sigLinkClicked(const QString&, const QString&, const QString&)),
            model(), SIGNAL(sigLinkClicked(const QString&, const QString&, const QString&)));
}

void UIGDetailsElement::prepareButton()
{
    /* Setup toggle-button: */
    m_buttonIcon = UIIconPool::iconSet(":/arrow_right_10px.png");
    m_pButton = new UIGraphicsRotatorButton(this, "additionalHeight", !m_fClosed, true /* reflected */);
    m_pButton->setIcon(m_buttonIcon);
    connect(m_pButton, SIGNAL(sigRotationStart()), this, SLOT(sltElementToggleStart()));
    connect(m_pButton, SIGNAL(sigRotationFinish(bool)), this, SLOT(sltElementToggleFinish(bool)));
}

void UIGDetailsElement::updateSizeHint()
{
    updateGeometry();
}

void UIGDetailsElement::updateLayout()
{
    /* Prepare variables: */
    QSize size = geometry().size().toSize();
    int iMargin = data(ElementData_Margin).toInt();
    QSize buttonSize = data(ElementData_ButtonSize).toSize();
    int iButtonWidth = buttonSize.width();
    int iButtonHeight = buttonSize.height();
    int iFullHeaderHeight = data(ElementData_HeaderSize).toSize().height();

    /* Layout button: */
    int iButtonX = size.width() - 2 * iMargin - iButtonWidth;
    int iButtonY = iButtonHeight == iFullHeaderHeight ? iMargin :
                   iMargin + (iFullHeaderHeight - iButtonHeight) / 2;
    m_pButton->setPos(iButtonX, iButtonY);
}

void UIGDetailsElement::setAdditionalHeight(int iAdditionalHeight)
{
    m_iAdditionalHeight = iAdditionalHeight;
    model()->updateLayout();
}

int UIGDetailsElement::additionalHeight() const
{
    return m_iAdditionalHeight;
}

UIGraphicsRotatorButton* UIGDetailsElement::button() const
{
    return m_pButton;
}

int UIGDetailsElement::minimumHeightHint(bool fClosed) const
{
    /* First of all, we have to prepare few variables: */
    int iMargin = data(ElementData_Margin).toInt();
    int iHeaderHeight = data(ElementData_HeaderSize).toSize().height();
    int iTextHeight = data(ElementData_TextSize).toSize().height();

    /* Calculating proposed height: */
    int iProposedHeight = 0;

    /* Two margins: */
    iProposedHeight += 2 * iMargin;

    /* Header height: */
    iProposedHeight += iHeaderHeight;

    /* Element is opened? */
    if (!fClosed)
    {
        /* Add text height: */
        if (!m_text.isEmpty())
            iProposedHeight += iMargin + iTextHeight;
    }
    else
    {
        /* Additional height during animation: */
        if (m_pButton->isAnimationRunning())
            iProposedHeight += m_iAdditionalHeight;
    }

    /* Return result: */
    return iProposedHeight;
}

QSizeF UIGDetailsElement::sizeHint(Qt::SizeHint which, const QSizeF &constraint /* = QSizeF() */) const
{
    /* If Qt::MinimumSize requested: */
    if (which == Qt::MinimumSize || which == Qt::PreferredSize)
    {
        /* Return wrappers: */
        return QSizeF(minimumWidthHint(), minimumHeightHint());
    }

    /* Call to base-class: */
    return UIGDetailsItem::sizeHint(which, constraint);
}

void UIGDetailsElement::paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, QWidget*)
{
    /* Update button visibility: */
    updateButtonVisibility();

    /* Configure painter shape: */
    configurePainterShape(pPainter, pOption, m_iCornerRadius);

    /* Paint decorations: */
    paintDecorations(pPainter, pOption);

    /* Paint machine info: */
    paintElementInfo(pPainter, pOption);
}

void UIGDetailsElement::paintDecorations(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption)
{
    /* Prepare variables: */
    QRect fullRect = pOption->rect;

    /* Paint background: */
    paintBackground(/* Painter: */
                    pPainter,
                    /* Rectangle to paint in: */
                    fullRect,
                    /* Rounded corners radius: */
                    m_iCornerRadius,
                    /* Header height: */
                    data(ElementData_Margin).toInt() +
                    data(ElementData_HeaderSize).toSize().height() +
                    1);

    /* Paint frame: */
    paintFrameRect(/* Painter: */
                   pPainter,
                   /* Rectangle to paint in: */
                   fullRect,
                   /* Rounded corner radius: */
                   m_iCornerRadius);
}

void UIGDetailsElement::paintElementInfo(QPainter *pPainter, const QStyleOptionGraphicsItem*)
{
    /* Initialize some necessary variables: */
    int iMargin = data(ElementData_Margin).toInt();
    int iSpacing = data(ElementData_Spacing).toInt();
    QSize pixmapSize = data(ElementData_PixmapSize).toSize();
    QSize nameSize = data(ElementData_NameSize).toSize();
    int iHeaderHeight = data(ElementData_HeaderSize).toSize().height();

    /* Calculate attributes: */
    int iPixmapHeight = pixmapSize.height();
    int iNameHeight = nameSize.height();
    int iMaximumHeight = qMax(iPixmapHeight, iNameHeight);

    /* Paint pixmap: */
    int iMachinePixmapX = 2 * iMargin;
    int iMachinePixmapY = iPixmapHeight == iMaximumHeight ?
                          iMargin : iMargin + (iMaximumHeight - iPixmapHeight) / 2;
    paintPixmap(/* Painter: */
                pPainter,
                /* Rectangle to paint in: */
                QRect(QPoint(iMachinePixmapX, iMachinePixmapY), pixmapSize),
                /* Pixmap to paint: */
                data(ElementData_Pixmap).value<QIcon>().pixmap(pixmapSize));

    /* Paint name: */
    int iMachineNameX = iMachinePixmapX +
                        pixmapSize.width() +
                        iSpacing;
    int iMachineNameY = iNameHeight == iMaximumHeight ?
                        iMargin : iMargin + (iMaximumHeight - iNameHeight) / 2;
    paintText(/* Painter: */
              pPainter,
              /* Rectangle to paint in: */
              QRect(QPoint(iMachineNameX, iMachineNameY), nameSize),
              /* Font to paint text: */
              data(ElementData_NameFont).value<QFont>(),
              /* Text to paint: */
              m_strName,
              /* Name hovered? */
              m_fNameHovered);

    /* Paint text: */
    if (!m_fClosed && !m_text.isEmpty())
    {
        /* Where to paint? */
        int iMachineTextX = iMachinePixmapX;
        int iMachineTextY = iMachinePixmapY + iHeaderHeight + iMargin;

        /* Font metrics: */
        QFontMetrics fm(data(ElementData_TextFont).value<QFont>());

        /* For each the line, get longest 'first': */
        int iLongestFirst = 0;
        foreach (const UITextTableLine line, m_text)
            iLongestFirst = qMax(iLongestFirst, fm.width(line.first));

        /* For each the line: */
        foreach (const UITextTableLine line, m_text)
        {
            /* Do we have a key-value pair? */
            bool fKeyValueRow = !line.second.isEmpty();

            /* First layout: */
            QTextLayout keyLayout(fKeyValueRow ? line.first + ":" : line.first);
            keyLayout.beginLayout();
            keyLayout.createLine();
            keyLayout.endLayout();
            keyLayout.draw(pPainter, QPointF(iMachineTextX, iMachineTextY));

            /* Second layout: */
            if (!line.second.isEmpty())
            {
                QTextLayout valueLayout(line.second);
                valueLayout.beginLayout();
                valueLayout.createLine();
                valueLayout.endLayout();
                valueLayout.draw(pPainter, QPointF(iMachineTextX + iLongestFirst + iSpacing, iMachineTextY));
            }

            /* Append the Y: */
            iMachineTextY += fm.height();
        }
    }
}

/* static */
void UIGDetailsElement::paintBackground(QPainter *pPainter, const QRect &rect, int iRadius, int iHeaderHeight)
{
    /* Save painter: */
    pPainter->save();

    /* Fill rectangle with white color: */
    QPalette pal = QApplication::palette();
    pPainter->fillRect(rect, Qt::white);

    /* Prepare color: */
    QColor windowColor = pal.color(QPalette::Active, QPalette::Window);

    /* Make even less rectangle: */
    QRect backGroundRect = rect;
    backGroundRect.setTopLeft(backGroundRect.topLeft() + QPoint(2, 2));
    backGroundRect.setBottomRight(backGroundRect.bottomRight() - QPoint(2, 2));
    /* Add even more clipping: */
    QPainterPath roundedPath;
    roundedPath.addRoundedRect(backGroundRect, iRadius, iRadius);
    pPainter->setClipPath(roundedPath);

    /* Calculate top rectangle: */
    QRect tRect = backGroundRect;
    tRect.setBottom(tRect.top() + iHeaderHeight);
    /* Calculate bottom rectangle: */
    QRect bRect = backGroundRect;
    bRect.setTop(tRect.bottom());

    /* Prepare top gradient: */
    QLinearGradient tGradient(tRect.bottomLeft(), tRect.topLeft());
    tGradient.setColorAt(0, windowColor.darker(115));
    tGradient.setColorAt(1, windowColor.darker(103));

    /* Paint all the stuff: */
    pPainter->fillRect(bRect, windowColor.darker(99));
    pPainter->fillRect(tRect, tGradient);

    /* Restore painter: */
    pPainter->restore();
}

void UIGDetailsElement::hoverMoveEvent(QGraphicsSceneHoverEvent *pEvent)
{
    /* Update hover state: */
    if (!m_fHovered)
    {
        m_fHovered = true;
        update();
    }

    /* Update name-hover state: */
    updateNameHoverRepresentation(pEvent);
}

void UIGDetailsElement::hoverLeaveEvent(QGraphicsSceneHoverEvent *pEvent)
{
    /* Update hover state: */
    if (m_fHovered)
    {
        m_fHovered = false;
        update();
    }

    /* Update name-hover state: */
    updateNameHoverRepresentation(pEvent);
}

void UIGDetailsElement::mousePressEvent(QGraphicsSceneMouseEvent *pEvent)
{
    if (m_fNameHovered)
    {
        pEvent->accept();
        QString strCategory;
        if (m_type >= DetailsElementType_General &&
            m_type <= DetailsElementType_SF)
            strCategory = QString("#%1").arg(gpConverter->toInternalString(m_type));
        else if (m_type == DetailsElementType_Description)
            strCategory = QString("#%1%%mTeDescription").arg(gpConverter->toInternalString(m_type));
        emit sigLinkClicked(strCategory, QString(), machine().GetId());
    }
}

void UIGDetailsElement::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *pEvent)
{
    /* Process left-button double-click: */
    if (pEvent->button() == Qt::LeftButton)
        emit sigToggleElement(m_type, closed());
}

void UIGDetailsElement::updateButtonVisibility()
{
    if (m_fHovered && !m_pButton->isVisible())
        m_pButton->show();
    else if (!m_fHovered && m_pButton->isVisible())
        m_pButton->hide();
}

void UIGDetailsElement::updateHoverAccessibility()
{
    /* Check if name-hovering should be available: */
    m_fNameHoveringAccessible = machine().isNull() || !machine().GetAccessible() ? false :
                                machine().GetState() != KMachineState_Stuck;
}

void UIGDetailsElement::updateNameHoverRepresentation(QGraphicsSceneHoverEvent *pEvent)
{
    /* Not for 'preview' element type: */
    if (m_type == DetailsElementType_Preview)
        return;

    /* Prepare variables: */
    int iMargin = data(ElementData_Margin).toInt();
    int iSpacing = data(ElementData_Spacing).toInt();
    int iPixmapWidth = data(ElementData_PixmapSize).toSize().width();
    QSize nameSize = data(ElementData_NameSize).toSize();
    int iNameHeight = nameSize.height();
    int iHeaderHeight = data(ElementData_HeaderSize).toSize().height();
    int iMachineNameX = 2 * iMargin + iPixmapWidth + iSpacing;
    int iMachineNameY = iNameHeight == iHeaderHeight ?
                        iMargin : iMargin + (iHeaderHeight - iNameHeight) / 2;

    /* Simulate hyperlink hovering: */
    QPoint point = pEvent->pos().toPoint();
    bool fNameHovered = QRect(QPoint(iMachineNameX, iMachineNameY), nameSize).contains(point);
    if (m_fNameHoveringAccessible && m_fNameHovered != fNameHovered)
    {
        m_fNameHovered = fNameHovered;
        if (m_fNameHovered)
            setCursor(Qt::PointingHandCursor);
        else
            unsetCursor();
        update();
    }
}

void UIGDetailsElement::updateAnimationParameters()
{
    /* Recalculate animation parameters: */
    int iOpenedHeight = minimumHeightHint(false);
    int iClosedHeight = minimumHeightHint(true);
    int iAdditionalHeight = iOpenedHeight - iClosedHeight;
    m_pButton->setAnimationRange(0, iAdditionalHeight);
}

