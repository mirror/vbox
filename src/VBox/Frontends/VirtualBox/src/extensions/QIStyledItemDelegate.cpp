/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QIStyledItemDelegate class implementation.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* GUI includes: */
#include "QIStyledItemDelegate.h"


QIStyledItemDelegate::QIStyledItemDelegate(QObject *pParent)
    : QStyledItemDelegate(pParent)
    , m_fWatchForEditorDataCommits(false)
    , m_fWatchForEditorEnterKeyTriggering(false)
{
}

void QIStyledItemDelegate::setWatchForEditorDataCommits(bool fWatch)
{
    m_fWatchForEditorDataCommits = fWatch;
}

void QIStyledItemDelegate::setWatchForEditorEnterKeyTriggering(bool fWatch)
{
    m_fWatchForEditorEnterKeyTriggering = fWatch;
}

QWidget *QIStyledItemDelegate::createEditor(QWidget *pParent,
                                            const QStyleOptionViewItem &option,
                                            const QModelIndex &index) const
{
    /* Call to base-class to get actual editor created: */
    QWidget *pEditor = QStyledItemDelegate::createEditor(pParent, option, index);

    /* Watch for editor data commits, redirect to listeners: */
    if (m_fWatchForEditorDataCommits)
        connect(pEditor, SIGNAL(sigCommitData(QWidget *)), this, SIGNAL(commitData(QWidget *)));

    /* Watch for editor Enter key triggering, redirect to listeners: */
    if (m_fWatchForEditorEnterKeyTriggering)
        connect(pEditor, SIGNAL(sigEnterKeyTriggered()), this, SIGNAL(sigEditorEnterKeyTriggered()));

    /* Notify listeners about editor created: */
    emit sigEditorCreated(pEditor, index);

    /* Return actual editor: */
    return pEditor;
}
