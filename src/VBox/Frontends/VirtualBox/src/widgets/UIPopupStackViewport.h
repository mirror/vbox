/* $Id$ */
/** @file
 * VBox Qt GUI - UIPopupStackViewport class declaration.
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

#ifndef ___UIPopupStackViewport_h___
#define ___UIPopupStackViewport_h___

/* Qt includes: */
#include <QWidget>
#include <QMap>

/* Forward declaration: */
class QSize;
class QString;
class UIPopupPane;

/** QWidget extension providing GUI with popup-stack viewport prototype class. */
class UIPopupStackViewport : public QWidget
{
    Q_OBJECT;

signals:

    /** Notifies about popup-pane size change. */
    void sigProposePopupPaneSize(QSize newSize);

    /** Notifies about size-hint change. */
    void sigSizeHintChanged();

    /** Asks to close popup-pane with @a strID and @a iResultCode. */
    void sigPopupPaneDone(QString strID, int iResultCode);
    /** Notifies about popup-pane with @a strID was removed. */
    void sigPopupPaneRemoved(QString strID);
    /** Notifies about popup-panes were removed. */
    void sigPopupPanesRemoved();

public:

    /** Constructs popup-stack viewport. */
    UIPopupStackViewport();

    /** Returns whether pane with passed @a strID exists. */
    bool exists(const QString &strID) const;
    /** Creates pane with passed @a strID, @a strMessage, @a strDetails and @a buttonDescriptions. */
    void createPopupPane(const QString &strID,
                         const QString &strMessage, const QString &strDetails,
                         const QMap<int, QString> &buttonDescriptions);
    /** Updates pane with passed @a strID, @a strMessage and @a strDetails. */
    void updatePopupPane(const QString &strID,
                         const QString &strMessage, const QString &strDetails);
    /** Recalls pane with passed @a strID. */
    void recallPopupPane(const QString &strID);

    /** Returns minimum size-hint. */
    QSize minimumSizeHint() const { return m_minimumSizeHint; }

public slots:

    /** Handle proposal for @a newSize. */
    void sltHandleProposalForSize(QSize newSize);

private slots:

    /** Adjusts geometry. */
    void sltAdjustGeometry();

    /** Handles reuqest to dismiss popup-pane with @a iButtonCode. */
    void sltPopupPaneDone(int iButtonCode);

private:

    /** Updates size-hint. */
    void updateSizeHint();
    /** Lays the content out. */
    void layoutContent();

    /** Holds the layout margin. */
    const int m_iLayoutMargin;
    /** Holds the layout spacing. */
    const int m_iLayoutSpacing;

    /** Holds the minimum size-hint. */
    QSize m_minimumSizeHint;

    /** Holds the popup-pane instances. */
    QMap<QString, UIPopupPane*> m_panes;
};

#endif /* !___UIPopupStackViewport_h___ */

