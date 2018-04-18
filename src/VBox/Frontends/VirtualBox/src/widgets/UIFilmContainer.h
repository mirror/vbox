/* $Id$ */
/** @file
 * VBox Qt GUI - UIFilmContainer class declaration.
 */

/*
 * Copyright (C) 2013-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIFilmContainer_h___
#define ___UIFilmContainer_h___

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"

/* Other VBox includes: */
#include <VBox/com/com.h>

/* Forward declarations: */
class QCheckBox;
class QScrollArea;
class QVBoxLayout;
class UIFilm;

/** QWidget subclass providing GUI with QScrollArea-based container for UIFilm widgets.
  * @todo Rename to something more suitable like UIScreenThumbnailContainer. */
class SHARED_LIBRARY_STUFF UIFilmContainer : public QWidget
{
    Q_OBJECT;

public:

    /** Constructs film-container passing @a pParent to the base-class. */
    UIFilmContainer(QWidget *pParent = 0);

    /** Returns the film-container check-box values. */
    QVector<BOOL> value() const;
    /** Defines the film-container check-box @a values. */
    void setValue(const QVector<BOOL> &values);

private:

    /** Prepares all. */
    void prepare();
    /** Prepares layout. */
    void prepareLayout();
    /** Prepares scroller. */
    void prepareScroller();

    /** Holds the main layout intance. */
    QVBoxLayout    *m_pMainLayout;
    /** Holds the scroller intance. */
    QScrollArea    *m_pScroller;
    /** Holds the list of film widgets. */
    QList<UIFilm*>  m_widgets;
};

#endif /* !___UIFilmContainer_h___ */
