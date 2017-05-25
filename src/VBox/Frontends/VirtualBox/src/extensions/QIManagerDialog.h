/* $Id$ */
/** @file
 * VBox Qt GUI - QIManagerDialog class declaration.
 */

/*
 * Copyright (C) 2009-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___QIManagerDialog_h___
#define ___QIManagerDialog_h___

/* Qt includes: */
#include <QMainWindow>

/* Forward declarations: */
class QIDialogButtonBox;
class QIManagerDialog;
#ifdef VBOX_WS_MAC
class UIToolBar;
#endif


/** Widget embedding type. */
enum EmbedTo
{
    EmbedTo_Dialog,
    EmbedTo_Stack
};


/** Manager dialog factory insterface. */
class QIManagerDialogFactory
{
public:

    /** Constructs Manager dialog factory. */
    QIManagerDialogFactory() {}
    /** Destructs Manager dialog factory. */
    virtual ~QIManagerDialogFactory() {}

    /** Prepares Manager dialog @a pDialog instance.
      * @param  pCenterWidget  Brings the widget reference to center according to. */
    virtual void prepare(QIManagerDialog *&pDialog, QWidget *pCenterWidget = 0);
    /** Cleanups Manager dialog @a pDialog instance. */
    virtual void cleanup(QIManagerDialog *&pDialog);

protected:

    /** Creates derived @a pDialog instance.
      * @param  pCenterWidget  Brings the widget reference to center according to. */
    virtual void create(QIManagerDialog *&pDialog, QWidget *pCenterWidget) = 0;
};


/** QMainWindow sub-class used as various manager dialogs. */
class QIManagerDialog : public QMainWindow
{
    Q_OBJECT;

signals:

    /** Notifies listener about dialog should be closed. */
    void sigClose();

protected:

    /** Constructs Manager dialog.
      * @param  pCenterWidget  Brings the widget reference to center according to. */
    QIManagerDialog(QWidget *pCenterWidget);

    /** @name Prepare/cleanup cascade.
      * @{ */
        /** Prepares dialog. */
        virtual void prepareDialog() {}
        /** Prepares widget. */
        virtual void prepareWidget() {}
    /** @} */

    /** @name Widget stuff.
      * @{ */
        /** Defines the @a pWidget instance. */
        void setWidget(QWidget *pWidget) { m_pWidget = pWidget; }
        /** Defines the @a pWidgetMenu instance. */
        void setWidgetMenu(QMenu *pWidgetMenu) { m_pWidgetMenu = pWidgetMenu; }
#ifdef VBOX_WS_MAC
        /** Defines the @a pWidgetToolbar instance. */
        void setWidgetToolbar(UIToolBar *pWidgetToolbar) { m_pWidgetToolbar = pWidgetToolbar; }
#endif
    /** @} */

    /** @name Event-handling stuff.
      * @{ */
        /** Handles close @a pEvent. */
        void closeEvent(QCloseEvent *pEvent);
    /** @} */

private:

    /** @name Prepare/cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares central-widget. */
        void prepareCentralWidget();
        /** Prepares button-box. */
        void prepareButtonBox();
        /** Prepares menu-bar. */
        void prepareMenuBar();
#ifdef VBOX_WS_MAC
        /** Prepares toolbar. */
        void prepareToolBar();
#endif

        /** Cleanup menu-bar. */
        void cleanupMenuBar();
        /** Cleanups all. */
        void cleanup();
    /** @} */

    /** @name General stuff.
      * @{ */
        /** Holds the widget reference to center Host Network Manager according. */
        QWidget *pCenterWidget;
    /** @} */

    /** @name Widget stuff.
      * @{ */
        /** Holds the widget instance. */
        QWidget *m_pWidget;

        /** Holds the widget menu instance. */
        QMenu     *m_pWidgetMenu;
#ifdef VBOX_WS_MAC
        /** Holds the widget toolbar instance. */
        UIToolBar *m_pWidgetToolbar;
#endif
    /** @} */

    /** @name Button-box stuff.
      * @{ */
        /** Holds the dialog button-box instance. */
        QIDialogButtonBox *m_pButtonBox;
    /** @} */

    /** Allow factory access to private/protected members: */
    friend class QIManagerDialogFactory;
};

#endif /* !___QIManagerDialog_h___ */

