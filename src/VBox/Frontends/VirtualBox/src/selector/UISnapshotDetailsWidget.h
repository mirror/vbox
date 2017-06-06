/* $Id$ */
/** @file
 * VBox Qt GUI - UISnapshotDetailsWidget class declaration.
 */

/*
 * Copyright (C) 2008-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UISnapshotDetailsWidget_h___
#define ___UISnapshotDetailsWidget_h___

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* COM includes: */
#include "CSnapshot.h"

/* Forward declarations: */
class QGridLayout;
class QLabel;
class QLineEdit;
class QStackedLayout;
class QTabWidget;
class QTextEdit;
class QVBoxLayout;
class QWidget;


/** Snapshot pane: Snapshot data structure. */
struct UIDataSnapshot
{
    /** Constructs data. */
    UIDataSnapshot()
        : m_strName(QString())
        , m_strDescription(QString())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSnapshot &other) const
    {
        return true
               && (m_strName == other.m_strName)
               && (m_strDescription == other.m_strDescription)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSnapshot &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSnapshot &other) const { return !equal(other); }

    /** Holds the snapshot name. */
    QString m_strName;
    /** Holds the snapshot description. */
    QString m_strDescription;
};


/** QWidget extension providing GUI with snapshot details-widget. */
class UISnapshotDetailsWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about data changed and whether it @a fDiffers. */
    void sigDataChanged(bool fDiffers);

public:

    /** Constructs snapshot details-widget passing @a pParent to the base-class. */
    UISnapshotDetailsWidget(QWidget *pParent = 0);

    /** Returns the snapshot data. */
    const UIDataSnapshot &data() const { return m_newData; }
    /** Defines the snapshot @a data. */
    void setData(const UIDataSnapshot &data, const CSnapshot &comSnapshot);
    /** Clears the snapshot data. */
    void clearData();

protected:

    /** Preprocesses any Qt @a pEvent for passed @a pObject. */
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Handles snapshot name change. */
    void sltHandleNameChange();
    /** Handles snapshot description change. */
    void sltHandleDescriptionChange();

private:

    /** Prepares all. */
    void prepare();
    /** Prepares empty-widget. */
    void prepareEmptyWidget();
    /** Prepares tab-widget. */
    void prepareTabWidget();
    /** Prepares 'Options' tab. */
    void prepareTabOptions();
    /** Prepares 'Details' tab. */
    void prepareTabDetails();

    /** Loads snapshot data. */
    void loadSnapshotData();

    /** Notifies listeners about data changed or not. */
    void notify();

    /** Returns a details report on a given @a comMachine. */
    QString detailsReport(const CMachine &comMachine);

    /** Holds the snapshot object to load data from. */
    CSnapshot  m_comSnapshot;

    /** Holds the old data copy. */
    UIDataSnapshot  m_oldData;
    /** Holds the new data copy. */
    UIDataSnapshot  m_newData;

    /** Holds the cached thumbnail. */
    QPixmap  m_pixmapThumbnail;
    /** Holds the cached screenshot. */
    QPixmap  m_pixmapScreenshot;

    /** Holds the stacked layout instance. */
    QStackedLayout *m_pStackedLayout;

    /** Holds the empty-widget instance. */
    QWidget *m_pEmptyWidget;
    /** Holds the empty-widget label instance. */
    QLabel  *m_pEmptyWidgetLabel;

    /** Holds the tab-widget instance. */
    QTabWidget *m_pTabWidget;

    /** Holds the 'Options' layout instance. */
    QGridLayout *m_pLayoutOptions;

    /** Holds the taken label instance. */
    QLabel *m_pLabelTaken;
    /** Holds the taken text instance. */
    QLabel *m_pLabelTakenText;

    /** Holds the name label instance. */
    QLabel    *m_pLabelName;
    /** Holds the name editor instance. */
    QLineEdit *m_pEditorName;

    /** Holds the thumbnail label instance. */
    QLabel *m_pLabelThumbnail;

    /** Holds the description label instance. */
    QLabel    *m_pLabelDescription;
    /** Holds the description editor instance. */
    QTextEdit *m_pBrowserDescription;

    /** Holds the 'Details' layout instance. */
    QVBoxLayout *m_pLayoutDetails;

    /** Holds the description editor instance. */
    QTextEdit *m_pBrowserDetails;
};

#endif /* !___UISnapshotDetailsWidget_h___ */

