/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: VBoxFilePathSelectorWidget class implementation
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "VBoxFilePathSelectorWidget.h"

/* Qt includes */
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFileIconProvider>

enum
{
    PathId = 0,
    SelectId,
    ResetId
};

VBoxFilePathSelectorWidget::VBoxFilePathSelectorWidget (QWidget *aParent /* = 0 */)
    : QIWithRetranslateUI<QComboBox> (aParent)
    , mIconProvider (new QFileIconProvider())
    , mCopyAction (new QAction (this))
    , mMode (PathMode)
{
    /* Populate items */
    insertItem (PathId, "");
    insertItem (SelectId, "");
    insertItem (ResetId, "");

    /* Setup context menu */
    addAction (mCopyAction);
    mCopyAction->setShortcut (QKeySequence (QKeySequence::Copy));
    mCopyAction->setShortcutContext (Qt::WidgetShortcut);

    /* Setup connections */
    connect (this, SIGNAL (activated (int)), this, SLOT (onActivated (int)));
    connect (mCopyAction, SIGNAL (triggered (bool)), this, SLOT (copyToClipboard()));

    /* Applying language settings */
    retranslateUi();

    /* Initial Setup */
    setContextMenuPolicy (Qt::ActionsContextMenu);
    setMinimumWidth (200);
    /* Set path to None */
    setPath (QString::null);
}

VBoxFilePathSelectorWidget::~VBoxFilePathSelectorWidget()
{
    delete mIconProvider;
}

void VBoxFilePathSelectorWidget::setMode (SelectorMode aMode)
{
    mMode = aMode;
}

VBoxFilePathSelectorWidget::SelectorMode VBoxFilePathSelectorWidget::mode() const
{
    return mMode;
}

void VBoxFilePathSelectorWidget::setResetEnabled (bool aEnabled)
{
    if (!aEnabled && count() - 1 == ResetId)
        removeItem (ResetId);
    else if (aEnabled && count() - 1 == ResetId - 1)
        insertItem (ResetId, "");
    retranslateUi();
}

bool VBoxFilePathSelectorWidget::isResetEnabled () const
{
    return (count() - 1  == ResetId);
}

void VBoxFilePathSelectorWidget::setNoneToolTip (const QString &aText)
{
    mNoneTip = aText;
    if (mPath.isNull())
    {
        setItemData (PathId, mNoneTip, Qt::ToolTipRole);
        setToolTip (mNoneTip);
    }
}

void VBoxFilePathSelectorWidget::setSelectToolTip (const QString &aText)
{
    setItemData (SelectId, aText, Qt::ToolTipRole);
}

void VBoxFilePathSelectorWidget::setResetToolTip (const QString &aText)
{
    setItemData (ResetId, aText, Qt::ToolTipRole);
}

bool VBoxFilePathSelectorWidget::isModified() const
{
    return true;
}

void VBoxFilePathSelectorWidget::setPath (const QString &aPath)
{
    mPath = aPath.isEmpty() ? QString::null : aPath;

    /* Attach corresponding icon */
    setItemIcon (PathId, QFileInfo (mPath).exists() ?
                         mIconProvider->icon (QFileInfo (mPath)) :
                         defaultIcon());

    /* Store full path as tooltip */
    setToolTip (filePath());
    setItemData (PathId, toolTip(), Qt::ToolTipRole);

    refreshText();
}

QString VBoxFilePathSelectorWidget::path() const
{
    return mPath;
}

void VBoxFilePathSelectorWidget::resizeEvent (QResizeEvent *aEvent)
{
    QIWithRetranslateUI<QComboBox>::resizeEvent (aEvent);
    refreshText();
}

void VBoxFilePathSelectorWidget::retranslateUi()
{
    /* Retranslate 'path' item */
    mNoneStr = tr ("None");
    if (mPath.isNull())
    {
        setItemText (PathId, mNoneStr);
        setItemData (PathId, mNoneTip, Qt::ToolTipRole);
        setToolTip (mNoneTip);
    }

    /* Retranslate 'select' item */
    setItemText (SelectId, tr ("Other..."));

    /* Retranslate 'reset' item */
    if (count() - 1 == ResetId)
        setItemText (ResetId, tr ("Reset"));

    /* Retranslate copy action */
    mCopyAction->setText (tr ("&Copy"));
}

void VBoxFilePathSelectorWidget::onActivated (int aIndex)
{
    switch (aIndex)
    {
        case SelectId:
        {
            emit selectPath();
            break;
        }
        case ResetId:
        {
            emit resetPath();
            break;
        }
        default:
            break;
    }
    setCurrentIndex (PathId);
}

void VBoxFilePathSelectorWidget::copyToClipboard()
{
    QString text = itemData (PathId, Qt::ToolTipRole).toString();
    QApplication::clipboard()->setText (text, QClipboard::Clipboard);
    QApplication::clipboard()->setText (text, QClipboard::Selection);
}

QIcon VBoxFilePathSelectorWidget::defaultIcon() const
{
    if (mMode == PathMode)
        return mIconProvider->icon (QFileIconProvider::Folder);
    else
        return mIconProvider->icon (QFileIconProvider::File);
}

QString VBoxFilePathSelectorWidget::filePath() const
{
    if (!mPath.isNull())
    {
        if (mMode == PathMode)
            return QDir (mPath).path();
        else
            return QFileInfo (mPath).filePath();
    }

    return mNoneTip;
}

QString VBoxFilePathSelectorWidget::shrinkText (int aWidth) const
{
    /* Full text stored in toolTip */
    QString fullText = toolTip();
    if (fullText.isEmpty())
        return fullText;

    int oldSize = fontMetrics().width (fullText);
    int indentSize = fontMetrics().width ("...x");

    /* Compress text */
    int start = 0;
    int finish = 0;
    int position = 0;
    int textWidth = 0;
    do {
        textWidth = fontMetrics().width (fullText);
        if (textWidth + indentSize > aWidth)
        {
            start = 0;
            finish = fullText.length();

            /* Selecting remove position */
            QRegExp regExp ("([\\\\/][^\\\\^/]+[\\\\/]?$)");
            int newFinish = regExp.indexIn (fullText);
            if (newFinish != -1)
                finish = newFinish;
            position = (finish - start) / 2;

            if (position == finish)
               break;

            fullText.remove (position, 1);
        }
    } while (textWidth + indentSize > aWidth);

    fullText.insert (position, "...");
    int newSize = fontMetrics().width (fullText);

    return newSize < oldSize ? fullText : toolTip();
}

void VBoxFilePathSelectorWidget::refreshText()
{
    if (mPath.isNull())
    {
        if (itemText (PathId) != mNoneStr)
            setItemText (PathId, mNoneStr);
    }
    else
    {
        /* Compress text in combobox */
        QStyleOptionComboBox options;
        options.initFrom (this);
        QRect rect = QApplication::style()->subControlRect (
            QStyle::CC_ComboBox, &options, QStyle::SC_ComboBoxEditField);
        setItemText (PathId, shrinkText (rect.width() - iconSize().width()));
    }
}

