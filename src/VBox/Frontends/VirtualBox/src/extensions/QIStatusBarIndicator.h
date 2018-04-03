/* $Id$ */
/** @file
 * VBox Qt GUI - QIStatusBarIndicator interface declaration.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___QIStatusBarIndicators_h___
#define ___QIStatusBarIndicators_h___

/* Qt includes: */
#include <QIcon>
#include <QMap>
#include <QWidget>

/* GUI includes: */
#include "UILibraryDefs.h"

/* Forward declarations: */
class QIcon;
class QLabel;
class QSize;
class QString;
class QWidget;


/** QWidget extension used as status-bar indicator. */
class SHARED_LIBRARY_STUFF QIStatusBarIndicator : public QWidget
{
    Q_OBJECT;

signals:

    /** Notifies about mouse-double-click-event: */
    void sigMouseDoubleClick(QIStatusBarIndicator *pIndicator, QMouseEvent *pEvent);
    /** Notifies about context-menu-request-event: */
    void sigContextMenuRequest(QIStatusBarIndicator *pIndicator, QContextMenuEvent *pEvent);

public:

    /** Constructs status-bar indicator passing @a pParent to the base-class. */
    QIStatusBarIndicator(QWidget *pParent = 0);

    /** Returns size-hint. */
    virtual QSize sizeHint() const { return m_size.isValid() ? m_size : QWidget::sizeHint(); }

protected:

#ifdef VBOX_WS_MAC
    /** Handles mouse-press @a pEvent. */
    virtual void mousePressEvent(QMouseEvent *pEvent) /* override */;
#endif /* VBOX_WS_MAC */
    /** Handles mouse-double-click @a pEvent. */
    virtual void mouseDoubleClickEvent(QMouseEvent *pEvent) /* override */;

    /** Handles context-menu @a pEvent. */
    virtual void contextMenuEvent(QContextMenuEvent *pEvent) /* override */;

    /** Holds currently cached size. */
    QSize m_size;
};


/** QIStatusBarIndicator extension used as status-bar state indicator. */
class SHARED_LIBRARY_STUFF QIStateStatusBarIndicator : public QIStatusBarIndicator
{
    Q_OBJECT;

public:

    /** Constructs state status-bar indicator passing @a pParent to the base-class. */
    QIStateStatusBarIndicator(QWidget *pParent = 0);

    /** Returns current state. */
    int state() const { return m_iState; }

    /** Returns state-icon for passed @a iState. */
    QIcon stateIcon(int iState) const;
    /** Defines state-icon for passed @a iState as @a icon. */
    void setStateIcon(int iState, const QIcon &icon);

public slots:

    /** Defines int @a state. */
    virtual void setState(int iState) { m_iState = iState; repaint(); }
    /** Defines bool @a state. */
    void setState(bool fState) { setState((int)fState); }

protected:

    /** Handles paint @a pEvent. */
    virtual void paintEvent(QPaintEvent *pEvent) /* override */;

    /** Draws contents using passed @a pPainter. */
    virtual void drawContents(QPainter *pPainter);

private:

    /** Holds current state. */
    int m_iState;
    /** Holds cached state icons. */
    QMap<int, QIcon> m_icons;
};


/** QIStatusBarIndicator extension used as status-bar state indicator. */
class SHARED_LIBRARY_STUFF QITextStatusBarIndicator : public QIStatusBarIndicator
{
    Q_OBJECT;

public:

    /** Constructs text status-bar indicator passing @a pParent to the base-class. */
    QITextStatusBarIndicator(QWidget *pParent = 0);

    /** Returns text. */
    QString text() const;
    /** Defines @a strText. */
    void setText(const QString &strText);

private:

    /** Holds the label instance. */
    QLabel *m_pLabel;
};


#endif /* !___QIStatusBarIndicators_h___ */

