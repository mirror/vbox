/* $Id$ */
/** @file
 * VBox Qt GUI - QIToolBar class declaration.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_extensions_QIToolBar_h
#define FEQT_INCLUDED_SRC_extensions_QIToolBar_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QToolBar>
#ifdef VBOX_WS_MAC
# include <QColor>
# include <QIcon>
#endif

/* GUI includes: */
#include "UILibraryDefs.h"

/* Forward declarations: */
class QMainWindow;
class QResizeEvent;
class QWidget;
#ifdef VBOX_WS_MAC
class QPaintEvent;
#endif

/** QToolBar extension with few settings presets. */
class SHARED_LIBRARY_STUFF QIToolBar : public QToolBar
{
    Q_OBJECT;

signals:

    /** Notifies listeners about @a newSize. */
    void sigResized(const QSize &newSize);

public:

    /** Constructs tool-bar passing @a pParent to the base-class. */
    QIToolBar(QWidget *pParent = 0);

    /** Defines whether tool-bar should use text-labels. */
    void setUseTextLabels(bool fEnable);
    /** Returns whether tool-bar should use text-labels. */
    bool useTextLabels() const;

#ifdef VBOX_WS_MAC
    /** Mac OS X: Defines whether native tool-bar should be enabled. */
    void enableMacToolbar();
    /** Mac OS X: Defines whether native tool-bar should be emulated. */
    void emulateMacToolbar();

    /** Mac OS X: Defines whether native tool-bar button should be shown. */
    void setShowToolBarButton(bool fShow);
    /** Mac OS X: Updates native tool-bar layout. */
    void updateLayout();

    /** Mac OS X: Defines branding stuff to be shown.
      * @param  icnBranding     Brings branding icon to be shown.
      * @param  strBranding     Brings branding text to be shown.
      * @param  clrBranding     Brings branding color to be used.
      * @param  iBrandingWidth  Holds the branding stuff width. */
    void enableBranding(const QIcon &icnBranding,
                        const QString &strBranding,
                        const QColor &clrBranding,
                        int iBrandingWidth);
#endif /* VBOX_WS_MAC */

protected:

    /** Handles @a pEvent. */
    virtual bool event(QEvent *pEvent) RT_OVERRIDE;

    /** Handles resize @a pEvent. */
    virtual void resizeEvent(QResizeEvent *pEvent) RT_OVERRIDE;

#ifdef VBOX_WS_MAC
    /** Handles paint @a pEvent. */
    virtual void paintEvent(QPaintEvent *pEvent) RT_OVERRIDE;
#endif

private:

    /** Prepares all. */
    void prepare();

#ifdef VBOX_WS_MAC
    /** Recalculates overall contents width. */
    void recalculateOverallContentsWidth();
#endif /* VBOX_WS_MAC */

    /** Holds the parent main-window isntance. */
    QMainWindow *m_pMainWindow;

#ifdef VBOX_WS_MAC
    /** Mac OS X: Holds whether unified tool-bar should be emulated. */
    bool  m_fEmulateUnifiedToolbar;

    /** Holds overall contents width. */
    int  m_iOverallContentsWidth;

    /** Mac OS X: Holds branding icon to be shown. */
    QIcon    m_icnBranding;
    /** Mac OS X: Holds branding text to be shown. */
    QString  m_strBranding;
    /** Mac OS X: Holds branding color to be used. */
    QColor   m_clrBranding;
    /** Mac OS X: Holds the branding stuff width. */
    int      m_iBrandingWidth;
#endif /* VBOX_WS_MAC */
};

#endif /* !FEQT_INCLUDED_SRC_extensions_QIToolBar_h */
