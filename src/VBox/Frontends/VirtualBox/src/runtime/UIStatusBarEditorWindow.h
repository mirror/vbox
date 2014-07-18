/** @file
 * VBox Qt GUI - UIStatusBarEditorWindow class declaration.
 */

/*
 * Copyright (C) 2010-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIStatusBarEditorWindow_h___
#define ___UIStatusBarEditorWindow_h___

/* Qt includes: */
#include <QWidget>

/* Forward declarations: */
class QMdiArea;
class QMdiSubWindow;
class QHBoxLayout;
class UIAnimation;
class UIMachineWindow;
class UIStatusBarEditorWidget;

/** QWidget reimplementation
  * providing user with possibility to edit status-bar layout. */
class UIStatusBarEditorWindow : public QWidget
{
    Q_OBJECT;
    Q_PROPERTY(QRect widgetGeometry READ widgetGeometry WRITE setWidgetGeometry);
    Q_PROPERTY(QRect startWidgetGeometry READ startWidgetGeometry);
    Q_PROPERTY(QRect finalWidgetGeometry READ finalWidgetGeometry);

signals:

    /** Notifies about window shown. */
    void sigShown();
    /** Commands window to expand. */
    void sigExpand();
    /** Commands window to collapse. */
    void sigCollapse();

public:

    /** Constructor, passes @a pParent to the QWidget constructor. */
    UIStatusBarEditorWindow(UIMachineWindow *pParent);

private slots:

    /** Performs window activation. */
    void sltActivateWindow() { activateWindow(); }

    /** Marks window as expanded. */
    void sltMarkAsExpanded() { m_fExpanded = true; }
    /** Marks window as collapsed. */
    void sltMarkAsCollapsed() { close(); m_fExpanded = false; }

    /** Handles parent geometry change. */
    void sltParentGeometryChanged(const QRect &rect);

private:

    /** Prepare routine. */
    void prepare();
    /** Prepare contents routine. */
    void prepareContents();
    /** Prepare geometry routine. */
    void prepareGeometry();
    /** Prepare animation routine. */
    void prepareAnimation();

    /** Update geometry. */
    void adjustGeometry();
    /** Updates animation. */
    void updateAnimation();

    /** Show event handler. */
    virtual void showEvent(QShowEvent *pEvent);
    /** Close event handler. */
    virtual void closeEvent(QCloseEvent *pEvent);

    /** Defines mdi-sub-window geometry. */
    void setWidgetGeometry(const QRect &rect);
    /** Returns mdi-sub-window geometry. */
    QRect widgetGeometry() const;
    /** Returns mdi-sub-window start-geometry. */
    QRect startWidgetGeometry() const { return m_startWidgetGeometry; }
    /** Returns mdi-sub-window final-geometry. */
    QRect finalWidgetGeometry() const { return m_finalWidgetGeometry; }

    /** @name Geometry
      * @{ */
        /** Holds the cached parent geometry. */
        QRect m_rect;
        /** Holds the cached status-bar geometry. */
        QRect m_statusBarRect;
    /** @} */

    /** @name Geometry: Animation
      * @{ */
        /** Holds the expand/collapse animation instance. */
        UIAnimation *m_pAnimation;
        /** Holds whether window is expanded. */
        bool m_fExpanded;
        /** Holds mdi-sub-window start-geometry. */
        QRect m_startWidgetGeometry;
        /** Holds mdi-sub-window final-geometry. */
        QRect m_finalWidgetGeometry;
    /** @} */

    /** @name Contents
      * @{ */
        /** Holds the main-layout instance. */
        QHBoxLayout *m_pMainLayout;
        /** */
        QMdiArea *m_pMdiArea;
        /** */
        UIStatusBarEditorWidget *m_pWidget;
        /** */
        QMdiSubWindow *m_pEmbeddedWidget;
    /** @} */
};

#endif /* !___UIStatusBarEditorWindow_h___ */
