/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QIManagerDialog class declaration.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_extensions_QIManagerDialog_h
#define FEQT_INCLUDED_SRC_extensions_QIManagerDialog_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QMainWindow>
#include <QMap>

/* GUI includes: */
#include "QIWithRestorableGeometry.h"
#include "UILibraryDefs.h"

/* Other VBox includes: */
#include <iprt/cdefs.h>

/* Forward declarations: */
class QPushButton;
class QIDialogButtonBox;
class QIManagerDialog;
#ifdef VBOX_WS_MAC
class QIToolBar;
#endif


/** Widget embedding type. */
enum EmbedTo
{
    EmbedTo_Dialog,
    EmbedTo_Stack
};


/** Dialog button types. */
enum ButtonType
{
    ButtonType_Invalid = 0,
    ButtonType_Reset   = RT_BIT(0),
    ButtonType_Apply   = RT_BIT(1),
    ButtonType_Close   = RT_BIT(2),
};


/** Manager dialog factory insterface. */
class SHARED_LIBRARY_STUFF QIManagerDialogFactory
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
class SHARED_LIBRARY_STUFF QIManagerDialog : public QIWithRestorableGeometry<QMainWindow>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about dialog should be closed. */
    void sigClose();

protected:

    /** Constructs Manager dialog.
      * @param  pCenterWidget  Brings the widget reference to center according to. */
    QIManagerDialog(QWidget *pCenterWidget);

    /** @name Virtual prepare/cleanup cascade.
      * @{ */
        /** Configures all.
          * @note Injected into prepare(), reimplement to configure all there. */
        virtual void configure() {}
        /** Configures central-widget.
          * @note Injected into prepareCentralWidget(), reimplement to configure central-widget there. */
        virtual void configureCentralWidget() {}
        /** Configures button-box.
          * @note Injected into prepareButtonBox(), reimplement to configure button-box there. */
        virtual void configureButtonBox() {}
        /** Performs final preparations.
          * @note Injected into prepare(), reimplement to postprocess all there. */
        virtual void finalize() {}
        /** Loads dialog setting from extradata. */
        virtual void loadSettings() {}

        /** Saves dialog setting into extradata. */
        virtual void saveSettings() {}
    /** @} */

    /** @name Widget stuff.
      * @{ */
        /** Defines the @a pWidget instance. */
        void setWidget(QWidget *pWidget) { m_pWidget = pWidget; }
        /** Defines the reference to widget menu, replacing current one. */
        void setWidgetMenu(QMenu *pWidgetMenu) { m_widgetMenus = QList<QMenu*>() << pWidgetMenu; }
        /** Defines the list of references to widget menus, replacing current one. */
        void setWidgetMenus(QList<QMenu*> widgetMenus) { m_widgetMenus = widgetMenus; }
#ifdef VBOX_WS_MAC
        /** Defines the @a pWidgetToolbar instance. */
        void setWidgetToolbar(QIToolBar *pWidgetToolbar) { m_pWidgetToolbar = pWidgetToolbar; }
#endif

        /** Returns the widget. */
        virtual QWidget *widget() { return m_pWidget; }
        /** Returns the button-box instance. */
        QIDialogButtonBox *buttonBox() { return m_pButtonBox; }
        /** Returns button of passed @a enmType. */
        QPushButton *button(ButtonType enmType) { return m_buttons.value(enmType); }
        /** Returns the widget reference to center manager dialog according. */
        QWidget *centerWidget() const { return m_pCenterWidget; }
    /** @} */

    /** @name Event-handling stuff.
      * @{ */
        /** Handles close @a pEvent. */
        virtual void closeEvent(QCloseEvent *pEvent) /* override */;
    /** @} */
    /** Sets the property 'helptag' of the button box's help button to @param strHelpTag and
      * sets up the appropirate connection. */
    void configureButtonBoxHelpButton(const QString &strHelpTag);

private:

    /** @name Private prepare/cleanup cascade.
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
        /** Holds the widget reference to center manager dialog according. */
        QWidget *m_pCenterWidget;

        /** Holds whether the manager had emitted command to be closed. */
        bool m_fCloseEmitted;
    /** @} */

    /** @name Widget stuff.
      * @{ */
        /** Holds the widget instance. */
        QWidget *m_pWidget;

        /** Holds a list of widget menu references. */
        QList<QMenu*> m_widgetMenus;

#ifdef VBOX_WS_MAC
        /** Holds the widget toolbar instance. */
        QIToolBar *m_pWidgetToolbar;
#endif
    /** @} */

    /** @name Button-box stuff.
      * @{ */
        /** Holds the dialog button-box instance. */
        QIDialogButtonBox *m_pButtonBox;

        /** Holds the button-box button references. */
        QMap<ButtonType, QPushButton*> m_buttons;
    /** @} */

    /** Allow factory access to private/protected members: */
    friend class QIManagerDialogFactory;
};


#endif /* !FEQT_INCLUDED_SRC_extensions_QIManagerDialog_h */
