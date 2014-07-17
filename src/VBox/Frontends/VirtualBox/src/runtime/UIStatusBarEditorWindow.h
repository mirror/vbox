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
#include <QList>
#include <QMap>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UIExtraDataDefs.h"

/* Forward declarations: */
class UIAnimation;
class QHBoxLayout;
class QIToolButton;
class UIStatusBarEditorButton;

/** QWidget reimplementation
  * providing user with possibility to edit status-bar layout. */
class UIStatusBarEditorWindow : public QIWithRetranslateUI2<QWidget>
{
    Q_OBJECT;
    Q_PROPERTY(QRect startGeometry READ startGeometry);
    Q_PROPERTY(QRect finalGeometry READ finalGeometry);

signals:

    /** Notifies about window shown. */
    void sigShown();
    /** Commands window to expand. */
    void sigExpand();
    /** Commands window to collapse. */
    void sigCollapse();

public:

    /** Constructor, passes @a pParent to the QIRichToolButton constructor.
      * @param rect is used to define initial cached parent geometry.
      * @param statusBarRect is used to define initial cached status-bar geometry. */
    UIStatusBarEditorWindow(QWidget *pParent, const QRect &rect, const QRect &statusBarRect);

private slots:

    /** Mark window as expanded. */
    void sltMarkAsExpanded() { m_fExpanded = true; }
    /** Mark window as collapsed. */
    void sltMarkAsCollapsed() { close(); m_fExpanded = false; }

    /** Handles parent geometry change. */
    void sltParentGeometryChanged(const QRect &rect);

    /** Handles configuration change. */
    void sltHandleConfigurationChange();

    /** Handles button click. */
    void sltHandleButtonClick();

    /** Handles drag object destroy. */
    void sltHandleDragObjectDestroy();

    /** Performs window activation. */
    void sltActivateWindow() { activateWindow(); }

private:

    /** Prepare routine. */
    void prepare();
    /** Prepare status buttons routine. */
    void prepareStatusButtons();
    /** Prepare status button routine. */
    void prepareStatusButton(IndicatorType type);
    /** Prepare animation routine. */
    void prepareAnimation();
    /** Prepare geometry. */
    void prepareGeometry();

    /** Updates status buttons. */
    void updateStatusButtons();
    /** Updates animation. */
    void updateAnimation();
    /** Update geometry. */
    void adjustGeometry();

    /** Retranslation routine. */
    virtual void retranslateUi();

    /** Show event handler. */
    virtual void showEvent(QShowEvent *pEvent);
    /** Close event handler. */
    virtual void closeEvent(QCloseEvent *pEvent);

    /** Paint event handler. */
    virtual void paintEvent(QPaintEvent *pEvent);

    /** Drag-enter event handler. */
    virtual void dragEnterEvent(QDragEnterEvent *pEvent);
    /** Drag-move event handler. */
    virtual void dragMoveEvent(QDragMoveEvent *pEvent);
    /** Drag-leave event handler. */
    virtual void dragLeaveEvent(QDragLeaveEvent *pEvent);
    /** Drop event handler. */
    virtual void dropEvent(QDropEvent *pEvent);

    /** Returns position for passed @a type. */
    int position(IndicatorType type) const;

    /** Returns cached start-geometry. */
    QRect startGeometry() const { return m_startGeometry; }
    /** Returns cached final-geometry. */
    QRect finalGeometry() const { return m_finalGeometry; }

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
        /** Holds the cached start-geometry. */
        QRect m_startGeometry;
        /** Holds the cached final-geometry. */
        QRect m_finalGeometry;
    /** @} */

    /** @name Contents
      * @{ */
        /** Holds the main-layout instance. */
        QHBoxLayout *m_pMainLayout;
        /** Holds the button-layout instance. */
        QHBoxLayout *m_pButtonLayout;
        /** Holds the close-button instance. */
        QIToolButton *m_pButtonClose;
        /** Holds status-bar buttons. */
        QMap<IndicatorType, UIStatusBarEditorButton*> m_buttons;
    /** @} */

    /** @name Contents: Restrictions
      * @{ */
        /** Holds the cached status-bar button restrictions. */
        QList<IndicatorType> m_restrictions;
    /** @} */

    /** @name Contents: Order
      * @{ */
        /** Holds the cached status-bar button order. */
        QList<IndicatorType> m_order;
        /** Holds the token-button to drop dragged-button nearby. */
        UIStatusBarEditorButton *m_pButtonDropToken;
        /** Holds whether dragged-button should be dropped <b>after</b> the token-button. */
        bool m_fDropAfterTokenButton;
    /** @} */
};

#endif /* !___UIStatusBarEditorWindow_h___ */
