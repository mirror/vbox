/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolsView class declaration.
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

#ifndef ___UIToolsView_h___
#define ___UIToolsView_h___

/* GUI includes: */
#include "QIGraphicsView.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class UITools;

/** QIGraphicsView extension used as VM Tools-pane view. */
class UIToolsView : public QIWithRetranslateUI<QIGraphicsView>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about resize. */
    void sigResized();

public:

    /** Constructs a Tools-view passing @a pParent to the base-class.
      * @param  pParent  Brings the Tools-container to embed into. */
    UIToolsView(UITools *pParent);

    /** @name General stuff.
      * @{ */
        /** Returns the Tools reference. */
        UITools *tools() const { return m_pTools; }
    /** @} */

public slots:

    /** @name General stuff.
      * @{ */
        /** Handles focus change to @a pFocusItem. */
        void sltFocusChanged();
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Handles minimum width @a iHint change. */
        void sltMinimumWidthHintChanged(int iHint);
        /** Handles minimum height @a iHint change. */
        void sltMinimumHeightHintChanged(int iHint);
    /** @} */

protected:

    /** @name Event handling stuff.
      * @{ */
        /** Handles translation event. */
        virtual void retranslateUi() /* override */;

        /** Handles resize @a pEvent. */
        virtual void resizeEvent(QResizeEvent *pEvent) /* override */;
    /** @} */

private:

    /** @name Prepare/Cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares palette. */
        void preparePalette();
    /** @} */

    /** @name General stuff.
      * @{ */
        /** Updates scene rectangle. */
        void updateSceneRect();
    /** @} */

    /** @name General stuff.
      * @{ */
        /** Holds the Tools-pane reference. */
        UITools *m_pTools;
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Holds the minimum width hint. */
        int m_iMinimumWidthHint;
        /** Holds the minimum height hint. */
        int m_iMinimumHeightHint;
    /** @} */
};

#endif /* !___UIToolsView_h___ */
