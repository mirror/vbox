/** @file
 * VBox Qt GUI - UIMiniToolBar class declaration (fullscreen/seamless).
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

#ifndef __UIMiniToolBar_h__
#define __UIMiniToolBar_h__

/* Qt includes: */
#include <QMainWindow>

/* GUI includes: */
#include "UIToolBar.h"

/* Forward declarations: */
class QTimer;
class QLabel;
class QMenu;
class QMdiArea;
class UIMiniToolBar;
class QMdiSubWindow;
class UIAnimation;

/** Geometry types. */
enum GeometryType
{
    GeometryType_Available,
    GeometryType_Full
};

/* Runtime mini-toolbar frameless-window prototype: */
class UIRuntimeMiniToolBar : public QWidget
{
    Q_OBJECT;
    Q_PROPERTY(QPoint toolbarPosition READ toolbarPosition WRITE setToolbarPosition);
    Q_PROPERTY(QPoint hiddenToolbarPosition READ hiddenToolbarPosition);
    Q_PROPERTY(QPoint shownToolbarPosition READ shownToolbarPosition);

signals:

    /* Notifiers: Action stuff: */
#ifndef RT_OS_DARWIN
    void sigMinimizeAction();
#endif /* !RT_OS_DARWIN */
    void sigExitAction();
    void sigCloseAction();

    /* Notifiers: Hover stuff: */
    void sigHoverEnter();
    void sigHoverLeave();

    /** Notifies listeners about we stole focus. */
    void sigNotifyAboutFocusStolen();

#ifdef VBOX_WITH_MASKED_SEAMLESS
# ifdef Q_WS_X11
    /** Notifies about geometry change. */
    void sigNotifyAboutGeometryChange(const QRect &geo);
# endif /* Q_WS_X11 */
#endif /* VBOX_WITH_MASKED_SEAMLESS */

public:

    /* Constructor/destructor: */
    UIRuntimeMiniToolBar(QWidget *pParent,
                         GeometryType geometryType,
                         Qt::Alignment alignment,
                         bool fAutoHide = true);
    ~UIRuntimeMiniToolBar();

    /* API: Alignment stuff: */
    void setAlignment(Qt::Alignment alignment);

    /* API: Auto-hide stuff: */
    bool autoHide() const { return m_fAutoHide; }
    void setAutoHide(bool fAutoHide, bool fPropagateToChild = true);

    /* API: Text stuff: */
    void setText(const QString &strText);

    /* API: Menu stuff: */
    void addMenus(const QList<QMenu*> &menus);

    /* API: Geometry stuff: */
    void adjustGeometry(int iHostScreen = -1);

private slots:

#ifdef RT_OS_DARWIN
    /** Handle 3D overlay visibility change. */
    void sltHandle3DOverlayVisibilityChange(bool fVisible) { if (fVisible) activateWindow(); }
#endif /* RT_OS_DARWIN */

    /* Handlers: Toolbar stuff: */
    void sltHandleToolbarResize();
    void sltAutoHideToggled();
    void sltHoverEnter();
    void sltHoverLeave();

private:

    /* Helpers: Prepare/cleanup stuff: */
    void prepare();
    void cleanup();

    /* Handlers: Event-processing stuff: */
    void enterEvent(QEvent *pEvent);
    void leaveEvent(QEvent *pEvent);

    /** Filters @a pEvent if <i>this</i> object has been
      * installed as an event-filter for the @a pWatched. */
    bool eventFilter(QObject *pWatched, QEvent *pEvent);

    /* Helper: Hover stuff: */
    void updateAutoHideAnimationBounds();
    void simulateToolbarAutoHiding();

    /* Property: Hover stuff: */
    void setToolbarPosition(QPoint point);
    QPoint toolbarPosition() const;
    QPoint hiddenToolbarPosition() const { return m_hiddenToolbarPosition; }
    QPoint shownToolbarPosition() const { return m_shownToolbarPosition; }

    /* Variables: General stuff: */
    const GeometryType m_geometryType;
    Qt::Alignment m_alignment;
    bool m_fAutoHide;

    /* Variables: Contents stuff: */
    QMdiArea *m_pMdiArea;
    UIMiniToolBar *m_pToolbar;
    QMdiSubWindow *m_pEmbeddedToolbar;

    /* Variables: Hover stuff: */
    bool m_fHovered;
    QTimer *m_pHoverEnterTimer;
    QTimer *m_pHoverLeaveTimer;
    QPoint m_hiddenToolbarPosition;
    QPoint m_shownToolbarPosition;
    UIAnimation *m_pAnimation;
};

/* Mini-toolbar widget prototype: */
class UIMiniToolBar : public UIToolBar
{
    Q_OBJECT;

signals:

    /* Notifier: Resize stuff: */
    void sigResized();

    /* Notifiers: Action stuff: */
    void sigAutoHideToggled();
#ifndef RT_OS_DARWIN
    void sigMinimizeAction();
#endif /* !RT_OS_DARWIN */
    void sigExitAction();
    void sigCloseAction();

public:

    /* Constructor: */
    UIMiniToolBar();

    /* API: Alignment stuff: */
    void setAlignment(Qt::Alignment alignment);

    /* API: Auto-hide stuff: */
    bool autoHide() const;
    void setAutoHide(bool fAutoHide);

    /* API: Text stuff: */
    void setText(const QString &strText);

    /* API: Menu aggregator: */
    void addMenus(const QList<QMenu*> &menus);

protected:

    /* Handlers: Event-processing stuff: */
    virtual void showEvent(QShowEvent *pEvent);
    virtual void polishEvent(QShowEvent *pEvent);
    virtual void resizeEvent(QResizeEvent *pEvent);
    virtual void paintEvent(QPaintEvent *pEvent);

private:

    /* Helper: Prepare stuff: */
    void prepare();

    /* Helper: Shape stuff: */
    void rebuildShape();

    /* Variables: General stuff: */
    bool m_fPolished;
    Qt::Alignment m_alignment;
    QPainterPath m_shape;

    /* Variables: Contents stuff: */
    QAction *m_pAutoHideAction;
    QLabel *m_pLabel;
#ifndef RT_OS_DARWIN
    QAction *m_pMinimizeAction;
#endif /* !RT_OS_DARWIN */
    QAction *m_pRestoreAction;
    QAction *m_pCloseAction;

    /* Variables: Menu stuff: */
    QAction *m_pMenuInsertPosition;

    /* Variables: Spacers stuff: */
    QList<QWidget*> m_spacings;
    QList<QWidget*> m_margins;
};

#endif // __UIMiniToolBar_h__

