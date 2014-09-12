/* $Id$ */
/** @file
 * VBox Qt GUI - UIMiniToolBar class implementation (fullscreen/seamless).
 */

/*
 * Copyright (C) 2009-2013 Oracle Corporation
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
# include <QApplication>
# include <QTimer>
# include <QMdiArea>
# include <QMdiSubWindow>
# include <QDesktopWidget>
# include <QLabel>
# include <QMenu>
# include <QToolButton>
# include <QStateMachine>
# include <QPainter>

/* GUI includes: */
# include "UIMiniToolBar.h"
# include "UIAnimationFramework.h"
# include "UIIconPool.h"
# include "VBoxGlobal.h"
# ifdef Q_WS_MAC
#  include "VBoxUtils-darwin.h"
# endif

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

#ifndef Q_WS_X11
# define VBOX_RUNTIME_UI_WITH_SHAPED_MINI_TOOLBAR
#endif /* !Q_WS_X11 */


UIRuntimeMiniToolBar::UIRuntimeMiniToolBar(QWidget *pParent,
                                           GeometryType geometryType,
                                           Qt::Alignment alignment,
                                           bool fAutoHide /* = true */)
    : QWidget(pParent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    /* Variables: General stuff: */
    , m_geometryType(geometryType)
    , m_alignment(alignment)
    , m_fAutoHide(fAutoHide)
    /* Variables: Contents stuff: */
    , m_pMdiArea(0)
    , m_pToolbar(0)
    , m_pEmbeddedToolbar(0)
    /* Variables: Hover stuff: */
    , m_fHovered(false)
    , m_pHoverEnterTimer(0)
    , m_pHoverLeaveTimer(0)
    , m_pAnimation(0)
{
    /* Prepare: */
    prepare();
}

UIRuntimeMiniToolBar::~UIRuntimeMiniToolBar()
{
    /* Cleanup: */
    cleanup();
}

void UIRuntimeMiniToolBar::setAlignment(Qt::Alignment alignment)
{
    /* Make sure alignment really changed: */
    if (m_alignment == alignment)
        return;

    /* Update alignment: */
    m_alignment = alignment;

    /* Re-initialize: */
    adjustGeometry();

    /* Propagate to child to update shape: */
    m_pToolbar->setAlignment(m_alignment);
}

void UIRuntimeMiniToolBar::setAutoHide(bool fAutoHide, bool fPropagateToChild /* = true */)
{
    /* Make sure auto-hide really changed: */
    if (m_fAutoHide == fAutoHide)
        return;

    /* Update auto-hide: */
    m_fAutoHide = fAutoHide;

    /* Re-initialize: */
    adjustGeometry();

    /* Propagate to child to update action if necessary: */
    if (fPropagateToChild)
        m_pToolbar->setAutoHide(m_fAutoHide);
}

void UIRuntimeMiniToolBar::setText(const QString &strText)
{
    /* Propagate to child: */
    m_pToolbar->setText(strText);
}

void UIRuntimeMiniToolBar::addMenus(const QList<QMenu*> &menus)
{
    /* Propagate to child: */
    m_pToolbar->addMenus(menus);
}

void UIRuntimeMiniToolBar::adjustGeometry(int iHostScreen /* = -1 */)
{
    /* This method could be called before parent-widget
     * become visible, we should skip everything in that case: */
    if (QApplication::desktop()->screenNumber(parentWidget()) == -1)
        return;

    /* Determine host-screen number if necessary: */
    if (iHostScreen == -1)
        iHostScreen = QApplication::desktop()->screenNumber(parentWidget());

    /* Reset toolbar geometry: */
    m_pEmbeddedToolbar->move(0, 0);
    m_pEmbeddedToolbar->resize(m_pEmbeddedToolbar->sizeHint());

    /* Adjust window geometry: */
    resize(m_pEmbeddedToolbar->size());
    QRect screenRect;
    int iX = 0, iY = 0;
    switch (m_geometryType)
    {
        case GeometryType_Available: screenRect = vboxGlobal().availableGeometry(iHostScreen); break;
        case GeometryType_Full:      screenRect = QApplication::desktop()->screenGeometry(iHostScreen); break;
        default: break;
    }
    iX = screenRect.x() + screenRect.width() / 2 - width() / 2;
    switch (m_alignment)
    {
        case Qt::AlignTop:    iY = screenRect.y(); break;
        case Qt::AlignBottom: iY = screenRect.y() + screenRect.height() - height(); break;
        default: break;
    }
    move(iX, iY);

    /* Recalculate auto-hide animation: */
    updateAutoHideAnimationBounds();

    /* Update toolbar geometry if necessary: */
    const QString strAnimationState = property("AnimationState").toString();
    if (strAnimationState == "Start")
        m_pEmbeddedToolbar->move(m_hiddenToolbarPosition);
    else if (strAnimationState == "Final")
        m_pEmbeddedToolbar->move(m_shownToolbarPosition);

    /* Simulate toolbar auto-hiding: */
    simulateToolbarAutoHiding();
}

void UIRuntimeMiniToolBar::sltHandleToolbarResize()
{
    /* Re-initialize: */
    adjustGeometry();
}

void UIRuntimeMiniToolBar::sltAutoHideToggled()
{
    /* Propagate from child: */
    setAutoHide(m_pToolbar->autoHide(), false);
}

void UIRuntimeMiniToolBar::sltHoverEnter()
{
    /* Mark as 'hovered' if necessary: */
    if (!m_fHovered)
    {
        m_fHovered = true;
        emit sigHoverEnter();
    }
}

void UIRuntimeMiniToolBar::sltHoverLeave()
{
    /* Mark as 'unhovered' if necessary: */
    if (m_fHovered)
    {
        m_fHovered = false;
        emit sigHoverLeave();
    }
}

void UIRuntimeMiniToolBar::prepare()
{
#ifdef VBOX_RUNTIME_UI_WITH_SHAPED_MINI_TOOLBAR
    /* Make sure we have no background
     * until the first one paint-event: */
    setAttribute(Qt::WA_NoSystemBackground);
# if defined(Q_WS_MAC)
    /* Using native API to enable translucent background:
     * - Under Mac host Qt doesn't allows to disable window-shadows
     *   until version 4.8, but minimum supported version is 4.7.1. */
    ::darwinSetShowsWindowTransparent(this, true);
# elif defined(Q_WS_WIN)
    /* Using Qt API to enable translucent background:
     * - Under Mac host Qt doesn't allows to disable window-shadows
     *   until version 4.8, but minimum supported version is 4.7.1.
     * - Under x11 host Qt has broken XComposite support (black background): */
    setAttribute(Qt::WA_TranslucentBackground);
# endif /* Q_WS_WIN */
#endif /* VBOX_RUNTIME_UI_WITH_SHAPED_MINI_TOOLBAR */

    /* Make sure we have no focus: */
    setFocusPolicy(Qt::NoFocus);

    /* Prepare mdi-area: */
    m_pMdiArea = new QMdiArea;
    {
        /* Allow any MDI area size: */
        m_pMdiArea->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        /* Configure own background: */
        QPalette pal = m_pMdiArea->palette();
        pal.setColor(QPalette::Window, QColor(Qt::transparent));
        m_pMdiArea->setPalette(pal);
        /* Configure viewport background: */
        m_pMdiArea->setBackground(QColor(Qt::transparent));
        /* Layout mdi-area according parent-widget: */
        QVBoxLayout *pMainLayout = new QVBoxLayout(this);
        pMainLayout->setContentsMargins(0, 0, 0, 0);
        pMainLayout->addWidget(m_pMdiArea);
        /* Make sure we have no focus: */
        m_pMdiArea->setFocusPolicy(Qt::NoFocus);
        m_pMdiArea->viewport()->setFocusPolicy(Qt::NoFocus);
    }

    /* Prepare mini-toolbar: */
    m_pToolbar = new UIMiniToolBar;
    {
        /* Make sure we have no focus: */
        m_pToolbar->setFocusPolicy(Qt::NoFocus);
        /* Propagate known options to child: */
        m_pToolbar->setAutoHide(m_fAutoHide);
        m_pToolbar->setAlignment(m_alignment);
        /* Configure own background: */
        QPalette pal = m_pToolbar->palette();
        pal.setColor(QPalette::Window, palette().color(QPalette::Window));
        m_pToolbar->setPalette(pal);
        /* Configure child connections: */
        connect(m_pToolbar, SIGNAL(sigResized()), this, SLOT(sltHandleToolbarResize()));
        connect(m_pToolbar, SIGNAL(sigAutoHideToggled()), this, SLOT(sltAutoHideToggled()));
#ifndef RT_OS_DARWIN
        connect(m_pToolbar, SIGNAL(sigMinimizeAction()), this, SIGNAL(sigMinimizeAction()));
#endif /* !RT_OS_DARWIN */
        connect(m_pToolbar, SIGNAL(sigExitAction()), this, SIGNAL(sigExitAction()));
        connect(m_pToolbar, SIGNAL(sigCloseAction()), this, SIGNAL(sigCloseAction()));
        /* Add child to mdi-area: */
        m_pEmbeddedToolbar = m_pMdiArea->addSubWindow(m_pToolbar, Qt::Window | Qt::FramelessWindowHint);
        /* Make sure we have no focus: */
        m_pEmbeddedToolbar->setFocusPolicy(Qt::NoFocus);
        m_pEmbeddedToolbar->installEventFilter(this);
    }

    /* Prepare hover-enter/leave timers: */
    m_pHoverEnterTimer = new QTimer(this);
    {
        m_pHoverEnterTimer->setSingleShot(true);
        m_pHoverEnterTimer->setInterval(50);
        connect(m_pHoverEnterTimer, SIGNAL(timeout()), this, SLOT(sltHoverEnter()));
    }
    m_pHoverLeaveTimer = new QTimer(this);
    {
        m_pHoverLeaveTimer->setSingleShot(true);
        m_pHoverLeaveTimer->setInterval(500);
        connect(m_pHoverLeaveTimer, SIGNAL(timeout()), this, SLOT(sltHoverLeave()));
    }

    /* Install 'auto-hide' animation to 'toolbarPosition' property: */
    m_pAnimation = UIAnimation::installPropertyAnimation(this,
                                                         "toolbarPosition",
                                                         "hiddenToolbarPosition", "shownToolbarPosition",
                                                         SIGNAL(sigHoverEnter()), SIGNAL(sigHoverLeave()),
                                                         true);

    /* Adjust geometry finally: */
    adjustGeometry();
}

void UIRuntimeMiniToolBar::cleanup()
{
    /* Stop hover-enter/leave timers: */
    if (m_pHoverEnterTimer->isActive())
        m_pHoverEnterTimer->stop();
    if (m_pHoverLeaveTimer->isActive())
        m_pHoverLeaveTimer->stop();

    /* Destroy animation before mdi-toolbar: */
    delete m_pAnimation;
    m_pAnimation = 0;

    /* Destroy mdi-toolbar after animation: */
    delete m_pEmbeddedToolbar;
    m_pEmbeddedToolbar = 0;
}

void UIRuntimeMiniToolBar::enterEvent(QEvent*)
{
    /* Stop the hover-leave timer if necessary: */
    if (m_pHoverLeaveTimer->isActive())
        m_pHoverLeaveTimer->stop();

    /* Start the hover-enter timer: */
    if (m_fAutoHide)
        m_pHoverEnterTimer->start();
}

void UIRuntimeMiniToolBar::leaveEvent(QEvent*)
{
    /* Stop the hover-enter timer if necessary: */
    if (m_pHoverEnterTimer->isActive())
        m_pHoverEnterTimer->stop();

    /* Start the hover-leave timer: */
    if (m_fAutoHide)
        m_pHoverLeaveTimer->start();
}

bool UIRuntimeMiniToolBar::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* Due to Qt bug QMdiArea can
     * 1. steal focus from current application focus-widget
     * 3. and even request focus stealing if QMdiArea hidden yet.
     * We have to notify listeners about such facts.. */
    if (pWatched && m_pEmbeddedToolbar && pWatched == m_pEmbeddedToolbar &&
        pEvent->type() == QEvent::FocusIn)
        emit sigNotifyAboutFocusStolen();
    /* Call to base-class: */
    return QWidget::eventFilter(pWatched, pEvent);
}

void UIRuntimeMiniToolBar::updateAutoHideAnimationBounds()
{
    /* Update animation: */
    m_shownToolbarPosition = m_pEmbeddedToolbar->pos();
    switch (m_alignment)
    {
        case Qt::AlignTop:
            m_hiddenToolbarPosition = m_shownToolbarPosition - QPoint(0, m_pEmbeddedToolbar->height() - 3);
            break;
        case Qt::AlignBottom:
            m_hiddenToolbarPosition = m_shownToolbarPosition + QPoint(0, m_pEmbeddedToolbar->height() - 3);
            break;
    }
    m_pAnimation->update();
}

void UIRuntimeMiniToolBar::simulateToolbarAutoHiding()
{
    /* This simulation helps user to notice
     * toolbar location, so it will be used only
     * 1. if toolbar unhovered and
     * 2. auto-hide feature enabled: */
    if (m_fHovered || !m_fAutoHide)
        return;

    /* Simulate hover-leave event: */
    m_fHovered = true;
    m_pHoverLeaveTimer->start();
}

void UIRuntimeMiniToolBar::setToolbarPosition(QPoint point)
{
    /* Update position: */
    AssertPtrReturnVoid(m_pEmbeddedToolbar);
    m_pEmbeddedToolbar->move(point);

#ifdef Q_WS_X11
    /* The setMask functionality is excessive under Win/Mac hosts
     * because there is a Qt composition works properly,
     * Mac host has native translucency support,
     * Win host allows to enable it through Qt::WA_TranslucentBackground: */
    setMask(m_pEmbeddedToolbar->geometry());
#endif /* Q_WS_X11 */
}

QPoint UIRuntimeMiniToolBar::toolbarPosition() const
{
    /* Return position: */
    AssertPtrReturn(m_pEmbeddedToolbar, QPoint());
    return m_pEmbeddedToolbar->pos();
}


UIMiniToolBar::UIMiniToolBar()
    /* Variables: General stuff: */
    : m_fPolished(false)
    , m_alignment(Qt::AlignBottom)
    /* Variables: Contents stuff: */
    , m_pAutoHideAction(0)
    , m_pLabel(0)
#ifndef RT_OS_DARWIN
    , m_pMinimizeAction(0)
#endif /* !RT_OS_DARWIN */
    , m_pRestoreAction(0)
    , m_pCloseAction(0)
    /* Variables: Menu stuff: */
    , m_pMenuInsertPosition(0)
{
    /* Prepare: */
    prepare();
}

void UIMiniToolBar::setAlignment(Qt::Alignment alignment)
{
    /* Make sure alignment really changed: */
    if (m_alignment == alignment)
        return;

    /* Update alignment: */
    m_alignment = alignment;

    /* Rebuild shape: */
    rebuildShape();
}

bool UIMiniToolBar::autoHide() const
{
    /* Return auto-hide: */
    return !m_pAutoHideAction->isChecked();
}

void UIMiniToolBar::setAutoHide(bool fAutoHide)
{
    /* Make sure auto-hide really changed: */
    if (m_pAutoHideAction->isChecked() == !fAutoHide)
        return;

    /* Update auto-hide: */
    m_pAutoHideAction->setChecked(!fAutoHide);
}

void UIMiniToolBar::setText(const QString &strText)
{
    /* Make sure text really changed: */
    if (m_pLabel->text() == strText)
        return;

    /* Update text: */
    m_pLabel->setText(strText);

    /* Resize to sizehint: */
    resize(sizeHint());
}

void UIMiniToolBar::addMenus(const QList<QMenu*> &menus)
{
    /* For each of the passed menu items: */
    for (int i = 0; i < menus.size(); ++i)
    {
        /* Get corresponding menu-action: */
        QAction *pAction = menus[i]->menuAction();
        /* Insert it into corresponding place: */
        insertAction(m_pMenuInsertPosition, pAction);
        /* Configure corresponding tool-button: */
        if (QToolButton *pButton = qobject_cast<QToolButton*>(widgetForAction(pAction)))
        {
            pButton->setPopupMode(QToolButton::InstantPopup);
            pButton->setAutoRaise(true);
        }
        /* Add some spacing: */
        if (i != menus.size() - 1)
            m_spacings << widgetForAction(insertWidget(m_pMenuInsertPosition, new QWidget(this)));
    }

    /* Resize to sizehint: */
    resize(sizeHint());
}

void UIMiniToolBar::showEvent(QShowEvent *pEvent)
{
    /* Make sure we should polish dialog: */
    if (m_fPolished)
        return;

    /* Call to polish-event: */
    polishEvent(pEvent);

    /* Mark dialog as polished: */
    m_fPolished = true;
}

void UIMiniToolBar::polishEvent(QShowEvent*)
{
    /* Toolbar spacings: */
    foreach(QWidget *pSpacing, m_spacings)
        pSpacing->setMinimumWidth(5);

    /* Title spacings: */
    foreach(QWidget *pLableMargin, m_margins)
        pLableMargin->setMinimumWidth(15);

    /* Resize to sizehint: */
    resize(sizeHint());
}

void UIMiniToolBar::resizeEvent(QResizeEvent*)
{
    /* Rebuild shape: */
    rebuildShape();

    /* Notify listeners: */
    emit sigResized();
}

void UIMiniToolBar::paintEvent(QPaintEvent*)
{
    /* Prepare painter: */
    QPainter painter(this);

    /* Fill background: */
    if (!m_shape.isEmpty())
    {
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setClipPath(m_shape);
    }
    QRect backgroundRect = rect();
    QColor backgroundColor = palette().color(QPalette::Window);
    QLinearGradient headerGradient(backgroundRect.bottomLeft(), backgroundRect.topLeft());
    headerGradient.setColorAt(0, backgroundColor.darker(120));
    headerGradient.setColorAt(1, backgroundColor.darker(90));
    painter.fillRect(backgroundRect, headerGradient);
}

void UIMiniToolBar::prepare()
{
    /* Configure toolbar: */
    setIconSize(QSize(16, 16));

#ifdef VBOX_RUNTIME_UI_WITH_SHAPED_MINI_TOOLBAR
    /* Left margin: */
    m_spacings << widgetForAction(addWidget(new QWidget));
#endif /* VBOX_RUNTIME_UI_WITH_SHAPED_MINI_TOOLBAR */

    /* Prepare push-pin: */
    m_pAutoHideAction = new QAction(this);
    m_pAutoHideAction->setIcon(UIIconPool::iconSet(":/pin_16px.png"));
    m_pAutoHideAction->setToolTip(tr("Always show the toolbar"));
    m_pAutoHideAction->setCheckable(true);
    connect(m_pAutoHideAction, SIGNAL(toggled(bool)), this, SIGNAL(sigAutoHideToggled()));
    addAction(m_pAutoHideAction);

    /* Left menu margin: */
    m_spacings << widgetForAction(addWidget(new QWidget));

    /* Right menu margin: */
    m_pMenuInsertPosition = addWidget(new QWidget);
    m_spacings << widgetForAction(m_pMenuInsertPosition);

    /* Left label margin: */
    m_margins << widgetForAction(addWidget(new QWidget));

    /* Insert a label for VM Name: */
    m_pLabel = new QLabel;
    m_pLabel->setAlignment(Qt::AlignCenter);
    addWidget(m_pLabel);

    /* Right label margin: */
    m_margins << widgetForAction(addWidget(new QWidget));

#ifndef RT_OS_DARWIN
    /* Minimize action: */
    m_pMinimizeAction = new QAction(this);
    m_pMinimizeAction->setIcon(UIIconPool::iconSet(":/minimize_16px.png"));
    m_pMinimizeAction->setToolTip(tr("Minimize Window"));
    connect(m_pMinimizeAction, SIGNAL(triggered()), this, SIGNAL(sigMinimizeAction()));
    addAction(m_pMinimizeAction);
#endif /* !RT_OS_DARWIN */

    /* Exit action: */
    m_pRestoreAction = new QAction(this);
    m_pRestoreAction->setIcon(UIIconPool::iconSet(":/restore_16px.png"));
    m_pRestoreAction->setToolTip(tr("Exit Full Screen or Seamless Mode"));
    connect(m_pRestoreAction, SIGNAL(triggered()), this, SIGNAL(sigExitAction()));
    addAction(m_pRestoreAction);

    /* Close action: */
    m_pCloseAction = new QAction(this);
    m_pCloseAction->setIcon(UIIconPool::iconSet(":/close_16px.png"));
    m_pCloseAction->setToolTip(tr("Close VM"));
    connect(m_pCloseAction, SIGNAL(triggered()), this, SIGNAL(sigCloseAction()));
    addAction(m_pCloseAction);

#ifdef VBOX_RUNTIME_UI_WITH_SHAPED_MINI_TOOLBAR
    /* Right margin: */
    m_spacings << widgetForAction(addWidget(new QWidget));
#endif /* VBOX_RUNTIME_UI_WITH_SHAPED_MINI_TOOLBAR */

    /* Resize to sizehint: */
    resize(sizeHint());
}

void UIMiniToolBar::rebuildShape()
{
#ifdef VBOX_RUNTIME_UI_WITH_SHAPED_MINI_TOOLBAR
    /* Rebuild shape: */
    QPainterPath shape;
    switch (m_alignment)
    {
        case Qt::AlignTop:
        {
            shape.moveTo(0, 0);
            shape.lineTo(shape.currentPosition().x(), height() - 10);
            shape.arcTo(QRectF(shape.currentPosition(), QSizeF(20, 20)).translated(0, -10), 180, 90);
            shape.lineTo(width() - 10, shape.currentPosition().y());
            shape.arcTo(QRectF(shape.currentPosition(), QSizeF(20, 20)).translated(-10, -20), 270, 90);
            shape.lineTo(shape.currentPosition().x(), 0);
            shape.closeSubpath();
            break;
        }
        case Qt::AlignBottom:
        {
            shape.moveTo(0, height());
            shape.lineTo(shape.currentPosition().x(), 10);
            shape.arcTo(QRectF(shape.currentPosition(), QSizeF(20, 20)).translated(0, -10), 180, -90);
            shape.lineTo(width() - 10, shape.currentPosition().y());
            shape.arcTo(QRectF(shape.currentPosition(), QSizeF(20, 20)).translated(-10, 0), 90, -90);
            shape.lineTo(shape.currentPosition().x(), height());
            shape.closeSubpath();
            break;
        }
        default:
            break;
    }
    m_shape = shape;

    /* Update: */
    update();
#endif /* VBOX_RUNTIME_UI_WITH_SHAPED_MINI_TOOLBAR */
}

