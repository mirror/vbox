/* $Id$ */
/** @file
 * VBox Qt GUI - UILanguageSettingsEditor class declaration.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_settings_editors_UILanguageSettingsEditor_h
#define FEQT_INCLUDED_SRC_settings_editors_UILanguageSettingsEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declartions: */
class QTreeWidgetItem;
class QILabelSeparator;
class QIRichTextLabel;
class QITreeWidget;

/** QWidget subclass used as a language settings editor. */
class SHARED_LIBRARY_STUFF UILanguageSettingsEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /** Constructs language settings editor passing @a pParent to the base-class. */
    UILanguageSettingsEditor(QWidget *pParent = 0);

    /** Defines editor @a strValue. */
    void setValue(const QString &strValue);
    /** Returns editor value. */
    QString value() const;

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Handles show @a pEvent. */
    virtual void showEvent(QShowEvent *pEvent) /* override */;
    /** Handles polish @a pEvent. */
    virtual void polishEvent(QShowEvent *pEvent) /* override */;

private slots:

    /** Handles @a pItem painting with passed @a pPainter. */
    void sltHandleItemPainting(QTreeWidgetItem *pItem, QPainter *pPainter);

    /** Handles @a pCurrentItem change. */
    void sltHandleCurrentItemChange(QTreeWidgetItem *pCurrentItem);

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares connections. */
    void prepareConnections();
    /** Cleanups all. */
    void cleanup();

    /** Reloads language list, choosing item with @a strLanguageId as current. */
    void reloadLanguageTree(const QString &strLanguageId);

    /** Holds whether the page is polished. */
    bool  m_fPolished;

    /** Holds the value to be set. */
    QString  m_strValue;

    /** @name Widgets
     * @{ */
        /** Holds the separator label instance. */
        QILabelSeparator *m_pLabelSeparator;
        /** Holds the tree-widget instance. */
        QITreeWidget     *m_pTreeWidget;
        /** Holds the info label instance. */
        QIRichTextLabel  *m_pLabelInfo;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_editors_UILanguageSettingsEditor_h */
