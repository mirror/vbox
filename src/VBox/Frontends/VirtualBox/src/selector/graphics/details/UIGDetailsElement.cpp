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
#include <QApplication>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QTextLayout>
#include <QGraphicsSceneMouseEvent>
#include <QStateMachine>
#include <QPropertyAnimation>
#include <QSignalTransition>

/* GUI includes: */
#include "UIGDetailsElement.h"
#include "UIGDetailsSet.h"
#include "UIGDetailsModel.h"
#include "UIGraphicsRotatorButton.h"
#include "UIIconPool.h"
#include "UIConverter.h"

UIGDetailsElement::UIGDetailsElement(UIGDetailsSet *pParent, DetailsElementType type, bool fOpened)
    : UIGDetailsItem(pParent)
    , m_pSet(pParent)
    , m_type(type)
    , m_fClosed(!fOpened)
    , m_pButton(0)
    , m_iAdditionalHeight(0)
    , m_iCornerRadius(10)
    , m_fHovered(false)
    , m_fNameHoveringAccessible(false)
    , m_fNameHovered(false)
    , m_pHighlightMachine(0)
    , m_pForwardAnimation(0)
    , m_pBackwardAnimation(0)
    , m_iAnimationDuration(400)
    , m_iDefaultDarkness(103)
    , m_iHighlightDarkness(90)
    , m_iGradient(m_iDefaultDarkness)
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
    /* Prepare variables: */
    int iMargin = data(ElementData_Margin).toInt();
    int iHeaderWidth = data(ElementData_HeaderSize).toSize().width();
    int iTextWidth = data(ElementData_TextWidth).toInt();

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

void UIGDetailsElement::updateHoverAccessibility()
{
    /* Check if name-hovering should be available: */
    m_fNameHoveringAccessible = machine().isNull() || !machine().GetAccessible() ? false :
                                machine().GetState() != KMachineState_Stuck;
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
        case ElementData_TextWidth:
        {
            /* Prepare variables: */
            int iSpacing = data(ElementData_Spacing).toInt();
            int iMinimumTextColumnWidth = data(ElementData_MinimumTextColumnWidth).toInt();
            QFontMetrics fm(data(ElementData_TextFont).value<QFont>());

            /* Search for the maximum line widths: */
            int iMaximumFirstLineWidth = 0;
            int iMaximumSecondLineWidth = 0;
            foreach (const UITextTableLine line, m_text)
            {
                bool fKeyValueRow = !line.second.isEmpty();
                QString strFirstLine = fKeyValueRow ? line.first + ":" : line.first;
                QString strSecondLine = line.second;
                iMaximumFirstLineWidth = qMax(iMaximumFirstLineWidth, fm.width(strFirstLine));
                iMaximumSecondLineWidth = qMax(iMaximumSecondLineWidth, fm.width(strSecondLine));
            }

            /* Calculate minimum text width: */
            int iMinimumSecondLineWidth = qMin(iMaximumSecondLineWidth, iMinimumTextColumnWidth);
            int iMinimumTextWidth = iMaximumFirstLineWidth + iSpacing + iMinimumSecondLineWidth;

            /* Return result: */
            return iMinimumTextWidth;
        }
        case ElementData_TextHeight:
        {
            /* Prepare variables: */
            int iMargin = data(ElementData_Margin).toInt();
            int iSpacing = data(ElementData_Spacing).toInt();
            int iMaximumTextWidth = (int)geometry().width() - 3 * iMargin - iSpacing;
            QFont textFont = data(ElementData_TextFont).value<QFont>();
            QFontMetrics fm(textFont);

            /* Search for maximum line widths: */
            int iMaximumFirstLineWidth = 0;
            int iMaximumSecondLineWidth = 0;
            foreach (const UITextTableLine line, m_text)
            {
                bool fKeyValueRow = !line.second.isEmpty();
                QString strFirstLine = fKeyValueRow ? line.first + ":" : line.first;
                QString strSecondLine = line.second;
                iMaximumFirstLineWidth = qMax(iMaximumFirstLineWidth, fm.width(strFirstLine));
                iMaximumSecondLineWidth = qMax(iMaximumSecondLineWidth, fm.width(strSecondLine));
            }
            iMaximumFirstLineWidth += 1;
            iMaximumSecondLineWidth += 1;

            /* Calculate column widths: */
            int iFirstColumnWidth = iMaximumFirstLineWidth;
            int iSecondColumnWidth = iMaximumTextWidth - iFirstColumnWidth;

            /* For each the line: */
            int iSummaryTextHeight = 0;
            foreach (const UITextTableLine line, m_text)
            {
                /* First layout: */
                int iFirstColumnHeight = 0;
                if (!line.first.isEmpty())
                {
                    bool fKeyValueRow = !line.second.isEmpty();
                    QTextLayout *pTextLayout = prepareTextLayout(textFont,
                                                                 fKeyValueRow ? line.first + ":" : line.first,
                                                                 iFirstColumnWidth, iFirstColumnHeight);
                    delete pTextLayout;
                }

                /* Second layout: */
                int iSecondColumnHeight = 0;
                if (!line.second.isEmpty())
                {
                    QTextLayout *pTextLayout = prepareTextLayout(textFont, line.second,
                                                                 iSecondColumnWidth, iSecondColumnHeight);
                    delete pTextLayout;
                }

                /* Append summary text height: */
                iSummaryTextHeight += qMax(iFirstColumnHeight, iSecondColumnHeight);
            }

            /* Return result: */
            return iSummaryTextHeight;
        }
        case ElementData_MinimumTextColumnWidth: return 100;
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

UITextTable UIGDetailsElement::text() const
{
    return m_text;
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
    /* Create highlight machine: */
    m_pHighlightMachine = new QStateMachine(this);
    /* Create 'default' state: */
    QState *pStateDefault = new QState(m_pHighlightMachine);
    /* Create 'highlighted' state: */
    QState *pStateHighlighted = new QState(m_pHighlightMachine);

    /* Forward animation: */
    m_pForwardAnimation = new QPropertyAnimation(this, "gradient", this);
    m_pForwardAnimation->setDuration(m_iAnimationDuration);
    m_pForwardAnimation->setStartValue(m_iDefaultDarkness);
    m_pForwardAnimation->setEndValue(m_iHighlightDarkness);

    /* Backward animation: */
    m_pBackwardAnimation = new QPropertyAnimation(this, "gradient", this);
    m_pBackwardAnimation->setDuration(m_iAnimationDuration);
    m_pBackwardAnimation->setStartValue(m_iHighlightDarkness);
    m_pBackwardAnimation->setEndValue(m_iDefaultDarkness);

    /* Add state transitions: */
    QSignalTransition *pDefaultToHighlighted = pStateDefault->addTransition(this, SIGNAL(sigHoverEnter()), pStateHighlighted);
    pDefaultToHighlighted->addAnimation(m_pForwardAnimation);
    QSignalTransition *pHighlightedToDefault = pStateHighlighted->addTransition(this, SIGNAL(sigHoverLeave()), pStateDefault);
    pHighlightedToDefault->addAnimation(m_pBackwardAnimation);

    /* Initial state is 'default': */
    m_pHighlightMachine->setInitialState(pStateDefault);
    /* Start state-machine: */
    m_pHighlightMachine->start();

    connect(this, SIGNAL(sigToggleElement(DetailsElementType, bool)), model(), SLOT(sltToggleElements(DetailsElementType, bool)));
    connect(this, SIGNAL(sigLinkClicked(const QString&, const QString&, const QString&)),
            model(), SIGNAL(sigLinkClicked(const QString&, const QString&, const QString&)));
}

void UIGDetailsElement::prepareButton()
{
    /* Setup toggle-button: */
    m_pButton = new UIGraphicsRotatorButton(this, "additionalHeight", !m_fClosed, true /* reflected */);
    connect(m_pButton, SIGNAL(sigRotationStart()), this, SLOT(sltElementToggleStart()));
    connect(m_pButton, SIGNAL(sigRotationFinish(bool)), this, SLOT(sltElementToggleFinish(bool)));
}

void UIGDetailsElement::updateLayout()
{
    /* Update size-hint: */
    updateSizeHint();

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
    /* Prepare variables: */
    int iMargin = data(ElementData_Margin).toInt();
    int iHeaderHeight = data(ElementData_HeaderSize).toSize().height();
    int iTextHeight = data(ElementData_TextHeight).toInt();

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
            iProposedHeight += 2 * iMargin + iTextHeight;
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
    /* Paint background: */
    paintBackground(/* Painter: */
                    pPainter,
                    /* Rectangle to paint in: */
                    pOption->rect,
                    /* Rounded corners radius: */
                    m_iCornerRadius,
                    /* Header height: */
                    2 * data(ElementData_Margin).toInt() +
                    data(ElementData_HeaderSize).toSize().height(),
                    /* Gradient color: */
                    gradient());
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
        /* Prepare variables: */
        int iMaximumTextWidth = geometry().width() - 3 * iMargin - iSpacing;
        QFont textFont = data(ElementData_TextFont).value<QFont>();
        QFontMetrics fm(textFont);

        /* Search for maximum line widths: */
        int iMaximumFirstLineWidth = 0;
        int iMaximumSecondLineWidth = 0;
        foreach (const UITextTableLine line, m_text)
        {
            bool fKeyValueRow = !line.second.isEmpty();
            QString strFirstLine = fKeyValueRow ? line.first + ":" : line.first;
            QString strSecondLine = line.second;
            iMaximumFirstLineWidth = qMax(iMaximumFirstLineWidth, fm.width(strFirstLine));
            iMaximumSecondLineWidth = qMax(iMaximumSecondLineWidth, fm.width(strSecondLine));
        }
        iMaximumFirstLineWidth += 1;
        iMaximumSecondLineWidth += 1;

        /* Calculate column widths: */
        int iFirstColumnWidth = iMaximumFirstLineWidth;
        int iSecondColumnWidth = iMaximumTextWidth - iFirstColumnWidth;

        /* Where to paint? */
        int iMachineTextX = iMachinePixmapX;
        int iMachineTextY = iMachinePixmapY + iHeaderHeight + 2 * iMargin;

        /* For each the line: */
        foreach (const UITextTableLine line, m_text)
        {
            /* First layout: */
            int iFirstColumnHeight = 0;
            if (!line.first.isEmpty())
            {
                bool fKeyValueRow = !line.second.isEmpty();
                QTextLayout *pTextLayout = prepareTextLayout(textFont,
                                                             fKeyValueRow ? line.first + ":" : line.first,
                                                             iFirstColumnWidth, iFirstColumnHeight);
                pTextLayout->draw(pPainter, QPointF(iMachineTextX, iMachineTextY));
                delete pTextLayout;
            }

            /* Second layout: */
            int iSecondColumnHeight = 0;
            if (!line.second.isEmpty())
            {
                QTextLayout *pTextLayout = prepareTextLayout(textFont, line.second,
                                                             iSecondColumnWidth, iSecondColumnHeight);
                pTextLayout->draw(pPainter, QPointF(iMachineTextX + iFirstColumnWidth + iSpacing, iMachineTextY));
                delete pTextLayout;
            }

            /* Indent Y: */
            iMachineTextY += qMax(iFirstColumnHeight, iSecondColumnHeight);
        }
    }
}

/* static */
void UIGDetailsElement::paintBackground(QPainter *pPainter, const QRect &rect,
                                        int iRadius, int iHeaderHeight, int iGradient)
{
    /* Save painter: */
    pPainter->save();

    /* Prepare variables: */
    int iFullHeight = rect.height();

    /* Prepare color: */
    QPalette pal = QApplication::palette();
    QColor windowColor = pal.color(QPalette::Active, QPalette::Window);

    /* Add clipping: */
    QPainterPath path;
    path.moveTo(iRadius, 0);
    path.arcTo(QRectF(path.currentPosition(), QSizeF(2 * iRadius, 2 * iRadius)).translated(-iRadius, 0), 90, 90);
    path.lineTo(path.currentPosition().x(), iFullHeight - iRadius);
    path.arcTo(QRectF(path.currentPosition(), QSizeF(2 * iRadius, 2 * iRadius)).translated(0, -iRadius), 180, 90);
    path.lineTo(rect.width() - iRadius, path.currentPosition().y());
    path.arcTo(QRectF(path.currentPosition(), QSizeF(2 * iRadius, 2 * iRadius)).translated(-iRadius, -2 * iRadius), 270, 90);
    path.lineTo(path.currentPosition().x(), iRadius);
    path.arcTo(QRectF(path.currentPosition(), QSizeF(2 * iRadius, 2 * iRadius)).translated(-2 * iRadius, -iRadius), 0, 90);
    path.closeSubpath();
    pPainter->setClipPath(path);

    /* Calculate top rectangle: */
    QRect tRect = rect;
    tRect.setBottom(tRect.top() + iHeaderHeight);
    /* Calculate bottom rectangle: */
    QRect bRect = rect;
    bRect.setTop(tRect.bottom());

    /* Prepare top gradient: */
    QLinearGradient tGradient(tRect.bottomLeft(), tRect.topLeft());
    tGradient.setColorAt(0, windowColor.darker(110));
    tGradient.setColorAt(1, windowColor.darker(iGradient));

    /* Paint all the stuff: */
    pPainter->fillRect(tRect, tGradient);
    pPainter->fillRect(bRect, windowColor.darker(97));

    /* Stroke path: */
    pPainter->setClipping(false);
    pPainter->strokePath(path, windowColor.darker(130));

    /* Restore painter: */
    pPainter->restore();
}

void UIGDetailsElement::hoverMoveEvent(QGraphicsSceneHoverEvent *pEvent)
{
    /* Update hover state: */
    if (!m_fHovered)
    {
        m_fHovered = true;
        emit sigHoverEnter();
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
        emit sigHoverLeave();
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

/* static  */
QTextLayout* UIGDetailsElement::prepareTextLayout(const QFont &font, const QString &strText, int iWidth, int &iHeight)
{
    /* Prepare variables: */
    QFontMetrics fm(font);
    int iLeading = fm.leading();

    /* Create layout; */
    QTextLayout *pTextLayout = new QTextLayout(strText, font);

    /* Configure layout: */
    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    pTextLayout->setTextOption(textOption);

    /* Build layout: */
    pTextLayout->beginLayout();
    while (1)
    {
        QTextLine line = pTextLayout->createLine();
        if (!line.isValid())
            break;

        line.setLineWidth(iWidth);
        iHeight += iLeading;
        line.setPosition(QPointF(0, iHeight));
        iHeight += line.height();
    }
    pTextLayout->endLayout();

    /* Return layout: */
    return pTextLayout;
}

void UIGDetailsElement::updateAnimationParameters()
{
    /* Recalculate animation parameters: */
    int iOpenedHeight = minimumHeightHint(false);
    int iClosedHeight = minimumHeightHint(true);
    int iAdditionalHeight = iOpenedHeight - iClosedHeight;
    m_pButton->setAnimationRange(0, iAdditionalHeight);
}

