/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserView class declaration.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_manager_chooser_UIChooserView_h
#define FEQT_INCLUDED_SRC_manager_chooser_UIChooserView_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIGraphicsView.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class UIChooser;
class UIChooserSearchWidget;

/** QIGraphicsView extension used as VM chooser pane view. */
class UIChooserView : public QIWithRetranslateUI<QIGraphicsView>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about resize. */
    void sigResized();

public:

    /** Constructs a chooser-view passing @a pParent to the base-class.
      * @param  pParent  Brings the chooser container to embed into. */
    UIChooserView(UIChooser *pParent);

    /** @name General stuff.
      * @{ */
        /** Returns the chooser reference. */
        UIChooser *chooser() const { return m_pChooser; }
    /** @} */

    /** @name Virtual Machine search stuff.
      * @{ */
        /** Returns if the search widget is visible or not. */
        bool isSearchWidgetVisible() const;
        /** Shows/hides wrt. @a fVisible machine search widget. */
        void setSearchWidgetVisible(bool fVisible);
        /** Updates the search widget's counts. */
        void setSearchResultsCount(int iTotalMacthCount, int iCurrentlyScrolledItemIndex);
        /** Forwards @a strSearchText to the search widget which in turn appends it to the current (if any) search term. */
        void appendToSearchString(const QString &strSearchText);
    /** @} */

public slots:

    /** @name Layout stuff.
      * @{ */
        /** Handles minimum width @a iHint change. */
        void sltMinimumWidthHintChanged(int iHint);
    /** @} */

protected:

    /** @name Event handling stuff.
      * @{ */
        /** Handles translation event. */
        virtual void retranslateUi() /* override */;

        /** Handles resize @a pEvent. */
        virtual void resizeEvent(QResizeEvent *pEvent) /* override */;
    /** @} */

private slots:

    /** Is connected to search widget's signal for a new search. */
    void sltRedoSearch(const QString &strSearchTerm, int iItemSearchFlags);
    /** Is connected to search widget's scroll to next/prev search result signal. */
    void sltHandleScrollToSearchResult(bool fIsNext);
    void sltHandleSearchWidgetVisibilityToggle(bool fIsVisible);

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

    /** @name Virtual Machine search stuff.
      * @{ */
        /** Updates search widget's geometry. */
        void updateSearchWidgetGeometry();
    /** @} */

    /** @name General stuff.
      * @{ */
        /** Holds the chooser pane reference. */
        UIChooser *m_pChooser;
        /** Holds the search widget instance reference. */
        UIChooserSearchWidget *m_pSearchWidget;
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Holds the minimum width hint. */
        int m_iMinimumWidthHint;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_manager_chooser_UIChooserView_h */
