/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserItemGlobal class implementation.
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
# include <QGraphicsSceneMouseEvent>
# include <QPainter>
# include <QStyleOptionGraphicsItem>

/* GUI includes: */
# include "VBoxGlobal.h"
# include "UIChooserItemGlobal.h"
# include "UIChooserModel.h"
# include "UIIconPool.h"
# include "UIVirtualBoxManager.h"

/* Other VBox includes: */
#include "iprt/cpp/utils.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIChooserItemGlobal::UIChooserItemGlobal(UIChooserItem *pParent,
                                         int iPosition /* = -1 */)
    : UIChooserItem(pParent, pParent->isTemporary())
    , m_iHoverLightness(0)
    , m_iHighlightLightness(0)
    , m_iHoverHighlightLightness(0)
    , m_iMinimumNameWidth(0)
    , m_iMaximumNameWidth(0)
    , m_iHeightHint(0)
{
    /* Prepare: */
    prepare();

    /* Add item to the parent: */
    AssertMsg(parentItem(), ("No parent set for global-item!"));
    parentItem()->addItem(this, iPosition);
    setZValue(parentItem()->zValue() + 1);

    /* Configure connections: */
    connect(gpManager, &UIVirtualBoxManager::sigWindowRemapped,
            this, &UIChooserItemGlobal::sltHandleWindowRemapped);

    /* Init: */
    updatePixmap();

    /* Apply language settings: */
    retranslateUi();
}

UIChooserItemGlobal::UIChooserItemGlobal(UIChooserItem *pParent,
                                         UIChooserItemGlobal * ,
                                         int iPosition /* = -1 */)
    : UIChooserItem(pParent, pParent->isTemporary())
    , m_iHoverLightness(0)
    , m_iHighlightLightness(0)
    , m_iHoverHighlightLightness(0)
    , m_iMinimumNameWidth(0)
    , m_iMaximumNameWidth(0)
{
    /* Prepare: */
    prepare();

    /* Add item to the parent: */
    AssertMsg(parentItem(), ("No parent set for global-item!"));
    parentItem()->addItem(this, iPosition);
    setZValue(parentItem()->zValue() + 1);

    /* Configure connections: */
    connect(gpManager, &UIVirtualBoxManager::sigWindowRemapped,
            this, &UIChooserItemGlobal::sltHandleWindowRemapped);

    /* Init: */
    updatePixmap();

    /* Apply language settings: */
    retranslateUi();
}

UIChooserItemGlobal::~UIChooserItemGlobal()
{
    /* If that item is focused: */
    if (model()->focusItem() == this)
    {
        /* Unset the focus: */
        model()->setFocusItem(0);
    }
    /* If that item is in selection list: */
    if (model()->currentItems().contains(this))
    {
        /* Remove item from the selection list: */
        model()->removeFromCurrentItems(this);
    }
    /* If that item is in navigation list: */
    if (model()->navigationList().contains(this))
    {
        /* Remove item from the navigation list: */
        model()->removeFromNavigationList(this);
    }

    /* Remove item from the parent: */
    AssertMsg(parentItem(), ("No parent set for global-item!"));
    parentItem()->removeItem(this);
}

void UIChooserItemGlobal::setHeightHint(int iHint)
{
    /* Remember a new hint: */
    m_iHeightHint = iHint;

    /* Update geometry and the model layout: */
    updateGeometry();
    model()->updateLayout();
}

void UIChooserItemGlobal::retranslateUi()
{
    /* Update description: */
    m_strName = tr("Global Tools");
    m_strDescription = m_strName;

    /* Update linked values: */
    updateMinimumNameWidth();
    updateVisibleName();

    /* Update tool-tip: */
    updateToolTip();
}

void UIChooserItemGlobal::showEvent(QShowEvent *pEvent)
{
    /* Call to base-class: */
    UIChooserItem::showEvent(pEvent);

    /* Update pixmaps: */
    updatePixmap();
}

void UIChooserItemGlobal::resizeEvent(QGraphicsSceneResizeEvent *pEvent)
{
    /* Call to base-class: */
    UIChooserItem::resizeEvent(pEvent);

    /* What is the new geometry? */
    const QRectF newGeometry = geometry();

    /* Should we update visible name? */
    if (previousGeometry().width() != newGeometry.width())
        updateMaximumNameWidth();

    /* Remember the new geometry: */
    setPreviousGeometry(newGeometry);
}

void UIChooserItemGlobal::mousePressEvent(QGraphicsSceneMouseEvent *pEvent)
{
    /* Call to base-class: */
    UIChooserItem::mousePressEvent(pEvent);
    /* No drag at all: */
    pEvent->ignore();
}

void UIChooserItemGlobal::paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions, QWidget * /* pWidget = 0 */)
{
    /* Acquire rectangle: */
    const QRect rectangle = pOptions->rect;

    /* Paint background: */
    paintBackground(pPainter, rectangle);
    /* Paint frame: */
    paintFrame(pPainter, rectangle);
    /* Paint global info: */
    paintGlobalInfo(pPainter, rectangle);
}

void UIChooserItemGlobal::startEditing()
{
    AssertMsgFailed(("Global graphics item do NOT support editing yet!"));
}

void UIChooserItemGlobal::updateToolTip()
{
//    setToolTip(toolTipText());
}

QString UIChooserItemGlobal::name() const
{
    return m_strName;
}

QString UIChooserItemGlobal::description() const
{
    return m_strDescription;
}

QString UIChooserItemGlobal::fullName() const
{
    return m_strName;
}

QString UIChooserItemGlobal::definition() const
{
    return QString("n=%1").arg("GLOBAL");
}

void UIChooserItemGlobal::addItem(UIChooserItem *, int)
{
    AssertMsgFailed(("Global graphics item do NOT support children!"));
}

void UIChooserItemGlobal::removeItem(UIChooserItem *)
{
    AssertMsgFailed(("Global graphics item do NOT support children!"));
}

void UIChooserItemGlobal::setItems(const QList<UIChooserItem*> &, UIChooserItemType)
{
    AssertMsgFailed(("Global graphics item do NOT support children!"));
}

QList<UIChooserItem*> UIChooserItemGlobal::items(UIChooserItemType) const
{
    AssertMsgFailedReturn(("Global graphics item do NOT support children!"), QList<UIChooserItem*>());
}

bool UIChooserItemGlobal::hasItems(UIChooserItemType) const
{
    AssertMsgFailedReturn(("Global graphics item do NOT support children!"), false);
}

void UIChooserItemGlobal::clearItems(UIChooserItemType)
{
    AssertMsgFailed(("Global graphics item do NOT support children!"));
}

void UIChooserItemGlobal::updateAllItems(const QString &)
{
    /* Update this global-item: */
    updatePixmap();
    updateToolTip();

    /* Update parent group-item: */
    parentItem()->updateToolTip();
    parentItem()->update();
}

void UIChooserItemGlobal::removeAllItems(const QString &)
{
    // Just do nothing ..
}

UIChooserItem *UIChooserItemGlobal::searchForItem(const QString &, int iItemSearchFlags)
{
    /* Ignoring if we are not searching for the global-item? */
    if (!(iItemSearchFlags & UIChooserItemSearchFlag_Global))
        return 0;

    /* Returning this: */
    return this;
}

UIChooserItem *UIChooserItemGlobal::firstMachineItem()
{
    return 0;
}

void UIChooserItemGlobal::sortItems()
{
    AssertMsgFailed(("Global graphics item do NOT support children!"));
}

void UIChooserItemGlobal::updateLayout()
{
    // Just do nothing ..
}

QSizeF UIChooserItemGlobal::sizeHint(Qt::SizeHint which, const QSizeF &constraint /* = QSizeF() */) const
{
    /* If Qt::MinimumSize requested: */
    if (which == Qt::MinimumSize)
        return QSizeF(minimumWidthHint(), minimumHeightHint());
    /* Else call to base-class: */
    return UIChooserItem::sizeHint(which, constraint);
}

int UIChooserItemGlobal::minimumWidthHint() const
{
    /* Prepare variables: */
    const int iMargin = data(GlobalItemData_Margin).toInt();
    const int iSpacing = data(GlobalItemData_Spacing).toInt();

    /* Calculating proposed width: */
    int iProposedWidth = 0;

    /* Two margins: */
    iProposedWidth += 2 * iMargin;
    /* And global-item content width: */
    iProposedWidth += (m_pixmapSize.width() +
                       iSpacing +
                       m_iMinimumNameWidth);

    /* Return result: */
    return iProposedWidth;
}

int UIChooserItemGlobal::minimumHeightHint() const
{
    /* Prepare variables: */
    const int iMargin = data(GlobalItemData_Margin).toInt();

    /* Calculating proposed height: */
    int iProposedHeight = 0;

    /* Global-item content height: */
    const int iContentHeight = qMax(m_pixmapSize.height(), m_visibleNameSize.height());

    /* If we have height hint: */
    if (m_iHeightHint)
    {
        /* Take the largest value between height hint and content height: */
        iProposedHeight += qMax(m_iHeightHint, iContentHeight);
    }
    /* Otherwise: */
    else
    {
        /* Two margins: */
        iProposedHeight += 2 * iMargin;
        /* And content height: */
        iProposedHeight += iContentHeight;
    }

    /* Return result: */
    return iProposedHeight;
}

QPixmap UIChooserItemGlobal::toPixmap()
{
    /* Ask item to paint itself into pixmap: */
    const QSize minimumSize = minimumSizeHint().toSize();
    QPixmap pixmap(minimumSize);
    QPainter painter(&pixmap);
    QStyleOptionGraphicsItem options;
    options.rect = QRect(QPoint(0, 0), minimumSize);
    paint(&painter, &options);
    return pixmap;
}

bool UIChooserItemGlobal::isDropAllowed(QGraphicsSceneDragDropEvent *, DragToken) const
{
    /* No drops at all: */
    return false;
}

void UIChooserItemGlobal::processDrop(QGraphicsSceneDragDropEvent *, UIChooserItem *, DragToken)
{
    /* Nothing to process: */
}

void UIChooserItemGlobal::resetDragToken()
{
    /* Nothing to process: */
}

QMimeData *UIChooserItemGlobal::createMimeData()
{
    /* Nothing to return: */
    return 0;
}

void UIChooserItemGlobal::sltHandleWindowRemapped()
{
    updatePixmap();
}

void UIChooserItemGlobal::prepare()
{
    /* Colors: */
#ifdef VBOX_WS_MAC
    m_iHighlightLightness = 115;
    m_iHoverLightness = 110;
    m_iHoverHighlightLightness = 120;
#else /* VBOX_WS_MAC */
    m_iHighlightLightness = 130;
    m_iHoverLightness = 155;
    m_iHoverHighlightLightness = 175;
#endif /* !VBOX_WS_MAC */

    /* Fonts: */
    m_nameFont = font();
    m_nameFont.setWeight(QFont::Bold);

    /* Sizes: */
    m_iMinimumNameWidth = 0;
    m_iMaximumNameWidth = 0;
}

QVariant UIChooserItemGlobal::data(int iKey) const
{
    /* Provide other members with required data: */
    switch (iKey)
    {
        /* Layout hints: */
        case GlobalItemData_Margin:  return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 3 * 2;
        case GlobalItemData_Spacing: return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 2;

        /* Default: */
        default: break;
    }
    return QVariant();
}

void UIChooserItemGlobal::updatePixmap()
{
    /* Acquire new metric, then compose pixmap-size: */
    const int iMetric = QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize);
    const QSize pixmapSize = QSize(iMetric, iMetric);

    /* Create new icon, then acquire pixmap: */
    const QIcon icon = UIIconPool::iconSet(":/tools_global_32px.png");
    const QPixmap pixmap = icon.pixmap(gpManager->windowHandle(), pixmapSize);

    /* Update linked values: */
    if (m_pixmapSize != pixmapSize)
    {
        m_pixmapSize = pixmapSize;
        updateMaximumNameWidth();
        updateGeometry();
    }
    if (m_pixmap.toImage() != pixmap.toImage())
    {
        m_pixmap = pixmap;
        update();
    }
}

void UIChooserItemGlobal::updateMinimumNameWidth()
{
    /* Calculate new minimum name width: */
    QPaintDevice *pPaintDevice = model()->paintDevice();
    const QFontMetrics fm(m_nameFont, pPaintDevice);
    const int iMinimumNameWidth = fm.width(compressText(m_nameFont, pPaintDevice, m_strName, textWidth(m_nameFont, pPaintDevice, 15)));

    /* Is there something changed? */
    if (m_iMinimumNameWidth == iMinimumNameWidth)
        return;

    /* Update linked values: */
    m_iMinimumNameWidth = iMinimumNameWidth;
    updateGeometry();
}

void UIChooserItemGlobal::updateMaximumNameWidth()
{
    /* Prepare variables: */
    const int iMargin = data(GlobalItemData_Margin).toInt();
    const int iSpacing = data(GlobalItemData_Spacing).toInt();

    /* Calculate new maximum name width: */
    int iMaximumNameWidth = (int)geometry().width();
    iMaximumNameWidth -= iMargin; /* left margin */
    iMaximumNameWidth -= m_pixmapSize.width(); /* pixmap width */
    iMaximumNameWidth -= iSpacing; /* spacing between pixmap and name */
    iMaximumNameWidth -= iMargin; /* right margin */

    /* Is there something changed? */
    if (m_iMaximumNameWidth == iMaximumNameWidth)
        return;

    /* Update linked values: */
    m_iMaximumNameWidth = iMaximumNameWidth;
    updateVisibleName();
}

void UIChooserItemGlobal::updateVisibleName()
{
    /* Prepare variables: */
    QPaintDevice *pPaintDevice = model()->paintDevice();

    /* Calculate new visible name and name-size: */
    const QString strVisibleName = compressText(m_nameFont, pPaintDevice, m_strName, m_iMaximumNameWidth);
    const QSize visibleNameSize = textSize(m_nameFont, pPaintDevice, strVisibleName);

    /* Update linked values: */
    if (m_visibleNameSize != visibleNameSize)
    {
        m_visibleNameSize = visibleNameSize;
        updateGeometry();
    }
    if (m_strVisibleName != strVisibleName)
    {
        m_strVisibleName = strVisibleName;
        update();
    }
}

void UIChooserItemGlobal::paintBackground(QPainter *pPainter, const QRect &rectangle) const
{
    /* Save painter: */
    pPainter->save();

    /* Prepare color: */
    const QPalette pal = palette();

    /* Selection background: */
    if (model()->currentItems().contains(unconst(this)))
    {
        /* Prepare color: */
        QColor highlight = pal.color(QPalette::Active, QPalette::Highlight);
        /* Draw gradient: */
        QLinearGradient bgGrad(rectangle.topLeft(), rectangle.bottomLeft());
        bgGrad.setColorAt(0, highlight.lighter(m_iHighlightLightness));
        bgGrad.setColorAt(1, highlight);
        pPainter->fillRect(rectangle, bgGrad);
    }
    /* Hovering background: */
    else if (isHovered())
    {
        /* Prepare color: */
        QColor highlight = pal.color(QPalette::Active, QPalette::Highlight);
        /* Draw gradient: */
        QLinearGradient bgGrad(rectangle.topLeft(), rectangle.bottomLeft());
        bgGrad.setColorAt(0, highlight.lighter(m_iHoverHighlightLightness));
        bgGrad.setColorAt(1, highlight.lighter(m_iHoverLightness));
        pPainter->fillRect(rectangle, bgGrad);
    }

    /* Restore painter: */
    pPainter->restore();
}

void UIChooserItemGlobal::paintFrame(QPainter *pPainter, const QRect &rectangle) const
{
    /* Only chosen and/or hovered item should have a frame: */
    if (!model()->currentItems().contains(unconst(this)) && !isHovered())
        return;

    /* Save painter: */
    pPainter->save();

    /* Prepare color: */
    const QPalette pal = palette();
    QColor strokeColor = pal.color(QPalette::Active,
                                   model()->currentItems().contains(unconst(this)) ?
                                   QPalette::Mid : QPalette::Highlight);

    /* Create/assign pen: */
    QPen pen(strokeColor);
    pen.setWidth(0);
    pPainter->setPen(pen);

    /* Draw rectangle: */
    pPainter->drawRect(rectangle);

    /* Restore painter: */
    pPainter->restore();
}

void UIChooserItemGlobal::paintGlobalInfo(QPainter *pPainter, const QRect &rectangle) const
{
    /* Prepare variables: */
    const int iFullHeight = rectangle.height();
    const int iMargin = data(GlobalItemData_Margin).toInt();
    const int iSpacing = data(GlobalItemData_Spacing).toInt();

    /* Selected item foreground: */
    if (model()->currentItems().contains(unconst(this)))
    {
        const QPalette pal = palette();
        pPainter->setPen(pal.color(QPalette::HighlightedText));
    }
    /* Hovered item foreground: */
    else if (isHovered())
    {
        /* Prepare color: */
        const QPalette pal = palette();
        const QColor highlight = pal.color(QPalette::Active, QPalette::Highlight);
        const QColor hhl = highlight.lighter(m_iHoverHighlightLightness);
        if (hhl.value() - hhl.saturation() > 0)
            pPainter->setPen(pal.color(QPalette::Active, QPalette::Text));
        else
            pPainter->setPen(pal.color(QPalette::Active, QPalette::HighlightedText));
    }

    /* Calculate indents: */
    int iLeftColumnIndent = iMargin;

    /* Paint left column: */
    {
        /* Prepare variables: */
        const int iGlobalPixmapX = iLeftColumnIndent;
        const int iGlobalPixmapY = (iFullHeight - m_pixmap.height() / m_pixmap.devicePixelRatio()) / 2;

        /* Paint pixmap: */
        paintPixmap(/* Painter: */
                    pPainter,
                    /* Point to paint in: */
                    QPoint(iGlobalPixmapX, iGlobalPixmapY),
                    /* Pixmap to paint: */
                    m_pixmap);
    }

    /* Calculate indents: */
    const int iRightColumnIndent = iLeftColumnIndent +
                                   m_pixmapSize.width() +
                                   iSpacing;

    /* Paint right column: */
    {
        /* Prepare variables: */
        const int iNameX = iRightColumnIndent;
        const int iNameY = (iFullHeight - m_visibleNameSize.height()) / 2;

        /* Paint name: */
        paintText(/* Painter: */
                  pPainter,
                  /* Point to paint in: */
                  QPoint(iNameX, iNameY),
                  /* Font to paint text: */
                  m_nameFont,
                  /* Paint device: */
                  model()->paintDevice(),
                  /* Text to paint: */
                  m_strVisibleName);
    }
}
