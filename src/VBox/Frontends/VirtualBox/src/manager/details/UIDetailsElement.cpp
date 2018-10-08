/* $Id$ */
/** @file
 * VBox Qt GUI - UIDetailsElement class implementation.
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
# include <QGraphicsView>
# include <QPropertyAnimation>
# include <QSignalTransition>
# include <QStateMachine>
# include <QStyleOptionGraphicsItem>

/* GUI includes: */
# include "UIActionPool.h"
# include "UIConverter.h"
# include "UIDetailsElement.h"
# include "UIDetailsSet.h"
# include "UIDetailsModel.h"
# include "UIGraphicsRotatorButton.h"
# include "UIGraphicsTextPane.h"
# include "UIIconPool.h"
# include "UIVirtualBoxManager.h"
# include "VBoxGlobal.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIDetailsElement::UIDetailsElement(UIDetailsSet *pParent, DetailsElementType enmType, bool fOpened)
    : UIDetailsItem(pParent)
    , m_pSet(pParent)
    , m_enmType(enmType)
#ifdef VBOX_WS_MAC
    , m_iDefaultToneStart(145)
    , m_iDefaultToneFinal(155)
    , m_iHoverToneStart(115)
    , m_iHoverToneFinal(125)
#else
    , m_iDefaultToneStart(160)
    , m_iDefaultToneFinal(190)
    , m_iHoverToneStart(160)
    , m_iHoverToneFinal(190)
#endif
    , m_fHovered(false)
    , m_fNameHovered(false)
    , m_pHoveringMachine(0)
    , m_pHoveringAnimationForward(0)
    , m_pHoveringAnimationBackward(0)
    , m_iAnimationDuration(300)
    , m_iDefaultValue(0)
    , m_iHoveredValue(255)
    , m_iAnimatedValue(m_iDefaultValue)
    , m_pButton(0)
    , m_fClosed(!fOpened)
    , m_fAnimationRunning(false)
    , m_iAdditionalHeight(0)
    , m_pTextPane(0)
    , m_iMinimumHeaderWidth(0)
    , m_iMinimumHeaderHeight(0)
{
    /* Prepare element: */
    prepareElement();
    /* Prepare button: */
    prepareButton();
    /* Prepare text-pane: */
    prepareTextPane();

    /* Setup size-policy: */
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    /* Add item to the parent: */
    AssertMsg(parentItem(), ("No parent set for details element!"));
    parentItem()->addItem(this);
}

UIDetailsElement::~UIDetailsElement()
{
    /* Remove item from the parent: */
    AssertMsg(parentItem(), ("No parent set for details element!"));
    parentItem()->removeItem(this);
}

void UIDetailsElement::setText(const UITextTable &text)
{
    /* Pass text to text-pane: */
    m_pTextPane->setText(text);
}

UITextTable &UIDetailsElement::text() const
{
    /* Retrieve text from text-pane: */
    return m_pTextPane->text();
}

void UIDetailsElement::close(bool fAnimated /* = true */)
{
    m_pButton->setToggled(false, fAnimated);
}

void UIDetailsElement::open(bool fAnimated /* = true */)
{
    m_pButton->setToggled(true, fAnimated);
}

void UIDetailsElement::markAnimationFinished()
{
    /* Mark animation as non-running: */
    m_fAnimationRunning = false;

    /* Recursively update size-hint: */
    updateGeometry();
    /* Repaint: */
    update();
}

void UIDetailsElement::updateAppearance()
{
    /* Reset name hover state: */
    m_fNameHovered = false;
    updateNameHoverLink();

    /* Update anchor role restrictions: */
    ConfigurationAccessLevel cal = m_pSet->configurationAccessLevel();
    m_pTextPane->setAnchorRoleRestricted("#mount", cal == ConfigurationAccessLevel_Null);
    m_pTextPane->setAnchorRoleRestricted("#attach", cal != ConfigurationAccessLevel_Full);
}

int UIDetailsElement::minimumWidthHint() const
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

int UIDetailsElement::minimumHeightHint() const
{
    return minimumHeightHintForElement(m_fClosed);
}

void UIDetailsElement::showEvent(QShowEvent *pEvent)
{
    /* Call to base-class: */
    UIDetailsItem::showEvent(pEvent);

    /* Update icon: */
    updateIcon();
}

void UIDetailsElement::resizeEvent(QGraphicsSceneResizeEvent*)
{
    /* Update layout: */
    updateLayout();
}

void UIDetailsElement::hoverMoveEvent(QGraphicsSceneHoverEvent *pEvent)
{
    /* Update hover state: */
    if (!m_fHovered)
    {
        m_fHovered = true;
        emit sigHoverEnter();
    }

    /* Update name-hover state: */
    handleHoverEvent(pEvent);
}

void UIDetailsElement::hoverLeaveEvent(QGraphicsSceneHoverEvent *pEvent)
{
    /* Update hover state: */
    if (m_fHovered)
    {
        m_fHovered = false;
        emit sigHoverLeave();
    }

    /* Update name-hover state: */
    handleHoverEvent(pEvent);
}

void UIDetailsElement::mousePressEvent(QGraphicsSceneMouseEvent *pEvent)
{
    /* Only for hovered header: */
    if (!m_fNameHovered)
        return;

    /* Process link click: */
    pEvent->accept();
    QString strCategory;
    if (m_enmType >= DetailsElementType_General &&
        m_enmType < DetailsElementType_Description)
        strCategory = QString("#%1").arg(gpConverter->toInternalString(m_enmType));
    else if (m_enmType == DetailsElementType_Description)
        strCategory = QString("#%1%%mTeDescription").arg(gpConverter->toInternalString(m_enmType));
    emit sigLinkClicked(strCategory, QString(), machine().GetId());
}

void UIDetailsElement::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *pEvent)
{
    /* Only for left-button: */
    if (pEvent->button() != Qt::LeftButton)
        return;

    /* Process left-button double-click: */
    emit sigToggleElement(m_enmType, isClosed());
}

void UIDetailsElement::paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions, QWidget *)
{
    /* Update button visibility: */
    updateButtonVisibility();

    /* Paint background: */
    paintBackground(pPainter, pOptions);
    /* Paint frame rectangle: */
    paintFrameRect(pPainter, pOptions);
    /* Paint element info: */
    paintElementInfo(pPainter, pOptions);
}

QString UIDetailsElement::description() const
{
    return tr("%1 details", "like 'General details' or 'Storage details'").arg(m_strName);
}

const CMachine &UIDetailsElement::machine()
{
    return m_pSet->machine();
}

void UIDetailsElement::setName(const QString &strName)
{
    /* Cache name: */
    m_strName = strName;
    QFontMetrics fm(m_nameFont, model()->paintDevice());
    m_nameSize = QSize(fm.width(m_strName), fm.height());

    /* Update linked values: */
    updateMinimumHeaderWidth();
    updateMinimumHeaderHeight();
}

void UIDetailsElement::setAdditionalHeight(int iAdditionalHeight)
{
    /* Cache new value: */
    m_iAdditionalHeight = iAdditionalHeight;
    /* Update layout: */
    updateLayout();
    /* Repaint: */
    update();
}

QVariant UIDetailsElement::data(int iKey) const
{
    /* Provide other members with required data: */
    switch (iKey)
    {
        /* Hints: */
        case ElementData_Margin: return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 4;
        case ElementData_Spacing: return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 2;
        /* Default: */
        default: break;
    }
    return QVariant();
}

void UIDetailsElement::addItem(UIDetailsItem*)
{
    AssertMsgFailed(("Details element do NOT support children!"));
}

void UIDetailsElement::removeItem(UIDetailsItem*)
{
    AssertMsgFailed(("Details element do NOT support children!"));
}

QList<UIDetailsItem*> UIDetailsElement::items(UIDetailsItemType) const
{
    AssertMsgFailed(("Details element do NOT support children!"));
    return QList<UIDetailsItem*>();
}

bool UIDetailsElement::hasItems(UIDetailsItemType) const
{
    AssertMsgFailed(("Details element do NOT support children!"));
    return false;
}

void UIDetailsElement::clearItems(UIDetailsItemType)
{
    AssertMsgFailed(("Details element do NOT support children!"));
}

void UIDetailsElement::updateLayout()
{
    /* Prepare variables: */
    QSize size = geometry().size().toSize();
    int iMargin = data(ElementData_Margin).toInt();

    /* Layout button: */
    int iButtonWidth = m_buttonSize.width();
    int iButtonHeight = m_buttonSize.height();
    int iButtonX = size.width() - 2 * iMargin - iButtonWidth;
    int iButtonY = iButtonHeight == m_iMinimumHeaderHeight ? iMargin :
                   iMargin + (m_iMinimumHeaderHeight - iButtonHeight) / 2;
    m_pButton->setPos(iButtonX, iButtonY);

    /* If closed: */
    if (isClosed())
    {
        /* Hide text-pane if still visible: */
        if (m_pTextPane->isVisible())
            m_pTextPane->hide();
    }
    /* If opened: */
    else
    {
        /* Layout text-pane: */
        int iTextPaneX = 2 * iMargin;
        int iTextPaneY = iMargin + m_iMinimumHeaderHeight + 2 * iMargin;
        m_pTextPane->setPos(iTextPaneX, iTextPaneY);
        m_pTextPane->resize(size.width() - 4 * iMargin,
                            size.height() - 4 * iMargin - m_iMinimumHeaderHeight);
        /* Show text-pane if still invisible and animation finished: */
        if (!m_pTextPane->isVisible() && !isAnimationRunning())
            m_pTextPane->show();
    }
}

int UIDetailsElement::minimumHeightHintForElement(bool fClosed) const
{
    /* Prepare variables: */
    int iMargin = data(ElementData_Margin).toInt();
    int iMinimumHeightHint = 0;

    /* Two margins: */
    iMinimumHeightHint += 2 * iMargin;

    /* Header height: */
    iMinimumHeightHint += m_iMinimumHeaderHeight;

    /* Element is opened? */
    if (!fClosed)
    {
        /* Add text height: */
        if (!m_pTextPane->isEmpty())
            iMinimumHeightHint += 2 * iMargin + (int)m_pTextPane->minimumSizeHint().height();
    }

    /* Additional height during animation: */
    if (m_fAnimationRunning)
        iMinimumHeightHint += m_iAdditionalHeight;

    /* Return value: */
    return iMinimumHeightHint;
}

void UIDetailsElement::sltHandleWindowRemapped()
{
    /* Update icon: */
    updateIcon();
}

void UIDetailsElement::sltToggleButtonClicked()
{
    emit sigToggleElement(m_enmType, isClosed());
}

void UIDetailsElement::sltElementToggleStart()
{
    /* Mark animation running: */
    m_fAnimationRunning = true;

    /* Setup animation: */
    updateAnimationParameters();

    /* Invert toggle-state: */
    m_fClosed = !m_fClosed;
}

void UIDetailsElement::sltElementToggleFinish(bool fToggled)
{
    /* Update toggle-state: */
    m_fClosed = !fToggled;

    /* Notify about finishing: */
    emit sigToggleElementFinished();
}

void UIDetailsElement::sltHandleAnchorClicked(const QString &strAnchor)
{
    /* Current anchor role: */
    const QString strRole = strAnchor.section(',', 0, 0);
    const QString strData = strAnchor.section(',', 1);

    /* Handle known anchor roles: */
    if (   strRole == "#mount"  // Optical and floppy attachments..
        || strRole == "#attach" // Hard-drive attachments..
        )
    {
        /* Prepare storage-menu: */
        UIMenu menu;
        menu.setShowToolTip(true);

        /* Storage-controller name: */
        QString strControllerName = strData.section(',', 0, 0);
        /* Storage-slot: */
        StorageSlot storageSlot = gpConverter->fromString<StorageSlot>(strData.section(',', 1));

        /* Fill storage-menu: */
        vboxGlobal().prepareStorageMenu(menu, this, SLOT(sltMountStorageMedium()),
                                        machine(), strControllerName, storageSlot);

        /* Exec menu: */
        menu.exec(QCursor::pos());
    }
}

void UIDetailsElement::sltMountStorageMedium()
{
    /* Sender action: */
    QAction *pAction = qobject_cast<QAction*>(sender());
    AssertMsgReturnVoid(pAction, ("This slot should only be called by menu action!\n"));

    /* Current mount-target: */
    const UIMediumTarget target = pAction->data().value<UIMediumTarget>();

    /* Update current machine mount-target: */
    vboxGlobal().updateMachineStorage(machine(), target);
}

void UIDetailsElement::prepareElement()
{
    /* Initialization: */
    m_nameFont = font();
    m_nameFont.setWeight(QFont::Bold);
    m_textFont = font();

    /* Update icon: */
    updateIcon();

    /* Create hovering animation machine: */
    m_pHoveringMachine = new QStateMachine(this);
    if (m_pHoveringMachine)
    {
        /* Create 'default' state: */
        QState *pStateDefault = new QState(m_pHoveringMachine);
        /* Create 'hovered' state: */
        QState *pStateHovered = new QState(m_pHoveringMachine);

        /* Configure 'default' state: */
        if (pStateDefault)
        {
            /* When we entering default state => we assigning animatedValue to m_iDefaultValue: */
            pStateDefault->assignProperty(this, "animatedValue", m_iDefaultValue);

            /* Add state transition: */
            QSignalTransition *pDefaultToHovered = pStateDefault->addTransition(this, SIGNAL(sigHoverEnter()), pStateHovered);
            if (pDefaultToHovered)
            {
                /* Create forward animation: */
                m_pHoveringAnimationForward = new QPropertyAnimation(this, "animatedValue", this);
                if (m_pHoveringAnimationForward)
                {
                    m_pHoveringAnimationForward->setDuration(m_iAnimationDuration);
                    m_pHoveringAnimationForward->setStartValue(m_iDefaultValue);
                    m_pHoveringAnimationForward->setEndValue(m_iHoveredValue);

                    /* Add to transition: */
                    pDefaultToHovered->addAnimation(m_pHoveringAnimationForward);
                }
            }
        }

        /* Configure 'hovered' state: */
        if (pStateHovered)
        {
            /* When we entering hovered state => we assigning animatedValue to m_iHoveredValue: */
            pStateHovered->assignProperty(this, "animatedValue", m_iHoveredValue);

            /* Add state transition: */
            QSignalTransition *pHoveredToDefault = pStateHovered->addTransition(this, SIGNAL(sigHoverLeave()), pStateDefault);
            if (pHoveredToDefault)
            {
                /* Create backward animation: */
                m_pHoveringAnimationBackward = new QPropertyAnimation(this, "animatedValue", this);
                if (m_pHoveringAnimationBackward)
                {
                    m_pHoveringAnimationBackward->setDuration(m_iAnimationDuration);
                    m_pHoveringAnimationBackward->setStartValue(m_iHoveredValue);
                    m_pHoveringAnimationBackward->setEndValue(m_iDefaultValue);

                    /* Add to transition: */
                    pHoveredToDefault->addAnimation(m_pHoveringAnimationBackward);
                }
            }
        }

        /* Initial state is 'default': */
        m_pHoveringMachine->setInitialState(pStateDefault);
        /* Start state-machine: */
        m_pHoveringMachine->start();
    }

    /* Configure connections: */
    connect(gpManager, &UIVirtualBoxManager::sigWindowRemapped,
            this, &UIDetailsElement::sltHandleWindowRemapped);
    connect(this, SIGNAL(sigToggleElement(DetailsElementType, bool)),
            model(), SLOT(sltToggleElements(DetailsElementType, bool)));
    connect(this, SIGNAL(sigLinkClicked(const QString&, const QString&, const QString&)),
            model(), SIGNAL(sigLinkClicked(const QString&, const QString&, const QString&)));
}

void UIDetailsElement::prepareButton()
{
    /* Setup toggle-button: */
    m_pButton = new UIGraphicsRotatorButton(this, "additionalHeight", !m_fClosed, true /* reflected */);
    m_pButton->setAutoHandleButtonClick(false);
    connect(m_pButton, SIGNAL(sigButtonClicked()), this, SLOT(sltToggleButtonClicked()));
    connect(m_pButton, SIGNAL(sigRotationStart()), this, SLOT(sltElementToggleStart()));
    connect(m_pButton, SIGNAL(sigRotationFinish(bool)), this, SLOT(sltElementToggleFinish(bool)));
    m_buttonSize = m_pButton->minimumSizeHint().toSize();
}

void UIDetailsElement::prepareTextPane()
{
    /* Create text-pane: */
    m_pTextPane = new UIGraphicsTextPane(this, model()->paintDevice());
    connect(m_pTextPane, SIGNAL(sigGeometryChanged()), this, SLOT(sltUpdateGeometry()));
    connect(m_pTextPane, SIGNAL(sigAnchorClicked(const QString&)), this, SLOT(sltHandleAnchorClicked(const QString&)));
}

void UIDetailsElement::updateIcon()
{
    /* Prepare whole icon first of all: */
    const QIcon icon = gpConverter->toIcon(elementType());

    /* Cache icon: */
    if (icon.isNull())
    {
        /* No icon provided: */
        m_pixmapSize = QSize();
        m_pixmap = QPixmap();
    }
    else
    {
        /* Determine default icon size: */
        const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
        m_pixmapSize = QSize(iIconMetric, iIconMetric);
        /* Acquire the icon of corresponding size (taking top-level widget DPI into account): */
        m_pixmap = icon.pixmap(gpManager->windowHandle(), m_pixmapSize);
    }

    /* Update linked values: */
    updateMinimumHeaderWidth();
    updateMinimumHeaderHeight();
}

void UIDetailsElement::handleHoverEvent(QGraphicsSceneHoverEvent *pEvent)
{
    /* Not for 'preview' element type: */
    if (m_enmType == DetailsElementType_Preview)
        return;

    /* Prepare variables: */
    int iMargin = data(ElementData_Margin).toInt();
    int iSpacing = data(ElementData_Spacing).toInt();
    int iNameHeight = m_nameSize.height();
    int iElementNameX = 2 * iMargin + m_pixmapSize.width() + iSpacing;
    int iElementNameY = iNameHeight == m_iMinimumHeaderHeight ?
                        iMargin : iMargin + (m_iMinimumHeaderHeight - iNameHeight) / 2;

    /* Simulate hyperlink hovering: */
    QPoint point = pEvent->pos().toPoint();
    bool fNameHovered = QRect(QPoint(iElementNameX, iElementNameY), m_nameSize).contains(point);
    if (   m_pSet->configurationAccessLevel() != ConfigurationAccessLevel_Null
        && m_fNameHovered != fNameHovered)
    {
        m_fNameHovered = fNameHovered;
        updateNameHoverLink();
    }
}

void UIDetailsElement::updateNameHoverLink()
{
    if (m_fNameHovered)
        VBoxGlobal::setCursor(this, Qt::PointingHandCursor);
    else
        VBoxGlobal::unsetCursor(this);
    update();
}

void UIDetailsElement::updateAnimationParameters()
{
    /* Recalculate animation parameters: */
    int iOpenedHeight = minimumHeightHintForElement(false);
    int iClosedHeight = minimumHeightHintForElement(true);
    int iAdditionalHeight = iOpenedHeight - iClosedHeight;
    if (m_fClosed)
        m_iAdditionalHeight = 0;
    else
        m_iAdditionalHeight = iAdditionalHeight;
    m_pButton->setAnimationRange(0, iAdditionalHeight);
}

void UIDetailsElement::updateButtonVisibility()
{
    if (m_fHovered && !m_pButton->isVisible())
        m_pButton->show();
    else if (!m_fHovered && m_pButton->isVisible())
        m_pButton->hide();
}

void UIDetailsElement::updateMinimumHeaderWidth()
{
    /* Prepare variables: */
    int iSpacing = data(ElementData_Spacing).toInt();

    /* Update minimum-header-width: */
    m_iMinimumHeaderWidth = m_pixmapSize.width() +
                            iSpacing + m_nameSize.width() +
                            iSpacing + m_buttonSize.width();
}

void UIDetailsElement::updateMinimumHeaderHeight()
{
    /* Update minimum-header-height: */
    m_iMinimumHeaderHeight = qMax(m_pixmapSize.height(), m_nameSize.height());
    m_iMinimumHeaderHeight = qMax(m_iMinimumHeaderHeight, m_buttonSize.height());
}

void UIDetailsElement::paintBackground(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions) const
{
    /* Save painter: */
    pPainter->save();

    /* Prepare variables: */
    const int iMargin = data(ElementData_Margin).toInt();
    const int iHeadHeight = 2 * iMargin + m_iMinimumHeaderHeight;
    const QRect optionRect = pOptions->rect;
    const QRect headRect = QRect(optionRect.topLeft(), QSize(optionRect.width(), iHeadHeight));
    const QRect fullRect = m_fAnimationRunning
                         ? QRect(optionRect.topLeft(), QSize(optionRect.width(), iHeadHeight + m_iAdditionalHeight))
                         : optionRect;

    /* Acquire palette: */
    const QPalette pal = palette();

    /* Paint default background: */
    const QColor defaultColor = pal.color(QPalette::Active, QPalette::Mid);
    const QColor dcTone1 = defaultColor.lighter(m_iDefaultToneFinal);
    const QColor dcTone2 = defaultColor.lighter(m_iDefaultToneStart);
    QLinearGradient gradientDefault(fullRect.topLeft(), fullRect.bottomLeft());
    gradientDefault.setColorAt(0, dcTone1);
    gradientDefault.setColorAt(1, dcTone2);
    pPainter->fillRect(fullRect, gradientDefault);

    /* If element is hovered: */
    if (m_fHovered)
    {
        /* Paint hovered background: */
        const QColor hoveredColor = pal.color(QPalette::Active, QPalette::Highlight);
        QColor hcTone1 = hoveredColor.lighter(m_iHoverToneFinal);
        QColor hcTone2 = hoveredColor.lighter(m_iHoverToneStart);
        hcTone1.setAlpha(m_iAnimatedValue);
        hcTone2.setAlpha(m_iAnimatedValue);
        QLinearGradient gradientHovered(headRect.topLeft(), headRect.bottomLeft());
        gradientHovered.setColorAt(0, hcTone1);
        gradientHovered.setColorAt(1, hcTone2);
        pPainter->fillRect(headRect, gradientHovered);
    }

    /* Restore painter: */
    pPainter->restore();
}

void UIDetailsElement::paintFrameRect(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions) const
{
    /* Save painter: */
    pPainter->save();

    /* Prepare variables: */
    const int iMargin = data(ElementData_Margin).toInt();
    const int iHeadHeight = 2 * iMargin + m_iMinimumHeaderHeight;
    const QRect optionRect = pOptions->rect;
    const QRect fullRect = m_fAnimationRunning
                         ? QRect(optionRect.topLeft(), QSize(optionRect.width(), iHeadHeight + m_iAdditionalHeight))
                         : optionRect;

    /* Paint frame: */
    const QColor strokeColor = palette().color(QPalette::Active, QPalette::Mid).lighter(m_iDefaultToneStart);
    QPen pen(strokeColor);
    pen.setWidth(0);
    pPainter->setPen(pen);
    pPainter->drawRect(fullRect);

    /* Restore painter: */
    pPainter->restore();
}

void UIDetailsElement::paintElementInfo(QPainter *pPainter, const QStyleOptionGraphicsItem *) const
{
    /* Initialize some necessary variables: */
    const int iMargin = data(ElementData_Margin).toInt();
    const int iSpacing = data(ElementData_Spacing).toInt();

    /* Calculate attributes: */
    const int iPixmapHeight = m_pixmapSize.height();
    const int iNameHeight = m_nameSize.height();
    const int iMaximumHeight = qMax(iPixmapHeight, iNameHeight);

    /* Prepare color: */
    const QPalette pal = palette();
    const QColor buttonTextColor = pal.color(QPalette::Active, QPalette::ButtonText);
    const QColor linkTextColor = pal.color(QPalette::Active, QPalette::Link);

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
              /* Name hovered? */
              m_fNameHovered ? linkTextColor : buttonTextColor);
}

/* static */
void UIDetailsElement::paintPixmap(QPainter *pPainter, const QRect &rect, const QPixmap &pixmap)
{
    pPainter->drawPixmap(rect, pixmap);
}

/* static */
void UIDetailsElement::paintText(QPainter *pPainter, QPoint point,
                                 const QFont &font, QPaintDevice *pPaintDevice,
                                 const QString &strText, const QColor &color)
{
    /* Prepare variables: */
    QFontMetrics fm(font, pPaintDevice);
    point += QPoint(0, fm.ascent());

    /* Draw text: */
    pPainter->save();
    pPainter->setFont(font);
    pPainter->setPen(color);
    pPainter->drawText(point, strText);
    pPainter->restore();
}
