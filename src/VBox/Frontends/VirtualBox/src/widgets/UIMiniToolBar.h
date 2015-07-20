/* $Id$ */
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
class UIMiniToolBarPrivate;

/** Geometry types. */
enum GeometryType
{
    GeometryType_Available,
    GeometryType_Full
};

/* Runtime mini-toolbar frameless-window prototype: */
class UIMiniToolBar : public QWidget
{
    Q_OBJECT;
    Q_PROPERTY(QPoint toolbarPosition READ toolbarPosition WRITE setToolbarPosition);
    Q_PROPERTY(QPoint hiddenToolbarPosition READ hiddenToolbarPosition);
    Q_PROPERTY(QPoint shownToolbarPosition READ shownToolbarPosition);

signals:

    /* Notifiers: Action stuff: */
    void sigMinimizeAction();
    void sigExitAction();
    void sigCloseAction();

    /* Notifiers: Hover stuff: */
    void sigHoverEnter();
    void sigHoverLeave();

    /** Notifies listeners about we stole focus. */
    void sigNotifyAboutFocusStolen();

public:

    /* Constructor/destructor: */
    UIMiniToolBar(QWidget *pParent,
                  GeometryType geometryType,
                  Qt::Alignment alignment,
                  bool fAutoHide = true);
    ~UIMiniToolBar();

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

#ifdef Q_WS_X11
    /** X11: Resize event handler. */
    void resizeEvent(QResizeEvent *pEvent);
#endif /* Q_WS_X11 */

    /** Filters @a pEvent if <i>this</i> object has been
      * installed as an event-filter for the @a pWatched. */
    bool eventFilter(QObject *pWatched, QEvent *pEvent);

    /* Helper: Hover stuff: */
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
    UIMiniToolBarPrivate *m_pToolbar;
    QMdiSubWindow *m_pEmbeddedToolbar;

    /* Variables: Hover stuff: */
    bool m_fHovered;
    QTimer *m_pHoverEnterTimer;
    QTimer *m_pHoverLeaveTimer;
    QPoint m_hiddenToolbarPosition;
    QPoint m_shownToolbarPosition;
    UIAnimation *m_pAnimation;
};

#endif // __UIMiniToolBar_h__

