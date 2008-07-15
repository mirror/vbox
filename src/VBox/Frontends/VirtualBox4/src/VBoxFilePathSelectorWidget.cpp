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
#include "VBoxGlobal.h"

/* Qt includes */
#include <QFileIconProvider>
#include <QDir>

#ifdef VBOX_USE_COMBOBOX_PATH_SELECTOR
# include <QComboBox>
#else /* VBOX_USE_COMBOBOX_PATH_SELECTOR */
# include <QLabel>
# include <QLineEdit>
# include <QToolButton>
#endif /* !VBOX_USE_COMBOBOX_PATH_SELECTOR */

enum 
{
    PathId = 0,
    SelectId,
    ResetId
};

VBoxFilePathSelectorWidget::VBoxFilePathSelectorWidget (QWidget *aParent /* = NULL */)
    : QIWithRetranslateUI<QWidget> (aParent)
    , mMode (PathMode)
{
    init();
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

void VBoxFilePathSelectorWidget::setLineEditWhatsThis (const QString &aText)
{
#ifdef VBOX_USE_COMBOBOX_PATH_SELECTOR
    NOREF (aText);
#else /* VBOX_USE_COMBOBOX_PATH_SELECTOR */
    mLePath->setWhatsThis (aText);
#endif /* !VBOX_USE_COMBOBOX_PATH_SELECTOR */
}

void VBoxFilePathSelectorWidget::setSelectorWhatsThis (const QString &aText)
{
#ifdef VBOX_USE_COMBOBOX_PATH_SELECTOR
    mCbPath->setItemData (SelectId, aText, Qt::WhatsThisRole);
#else /* VBOX_USE_COMBOBOX_PATH_SELECTOR */
    mTbSelect->setWhatsThis (aText);
#endif /* !VBOX_USE_COMBOBOX_PATH_SELECTOR */
}

void VBoxFilePathSelectorWidget::setResetWhatsThis (const QString &aText)
{
#ifdef VBOX_USE_COMBOBOX_PATH_SELECTOR
    mCbPath->setItemData (ResetId, aText, Qt::WhatsThisRole);
#else /* VBOX_USE_COMBOBOX_PATH_SELECTOR */
    mTbReset->setWhatsThis (aText);
#endif /* !VBOX_USE_COMBOBOX_PATH_SELECTOR */
}

bool VBoxFilePathSelectorWidget::isModified() const
{
#ifdef VBOX_USE_COMBOBOX_PATH_SELECTOR
    return true;
#else /* VBOX_USE_COMBOBOX_PATH_SELECTOR */
    return mLePath->isModified();
#endif /* !VBOX_USE_COMBOBOX_PATH_SELECTOR */
}

void VBoxFilePathSelectorWidget::setPath (const QString &aPath)
{
    mPath = aPath;
    QString tmpPath (aPath);
    QIcon icon;
    QFileInfo fi (tmpPath);
    if (fi.exists())
        icon = mIconProvider->icon (fi);
    else
        icon = defaultIcon();
    if (tmpPath.isEmpty())
        tmpPath = mNoneStr;
#ifdef VBOX_USE_COMBOBOX_PATH_SELECTOR
    mCbPath->setItemText (PathId, filePath (tmpPath, true));
    mCbPath->setItemIcon (PathId, icon);
#else /* VBOX_USE_COMBOBOX_PATH_SELECTOR */
    /* Do this instead of le->setText (folder) to cause
     * isModified() return true */
    mLePath->selectAll();
    mLePath->insert (filePath (tmpPath, false));
    mLbIcon->setPixmap (icon.pixmap (16, 16));
#endif /* !VBOX_USE_COMBOBOX_PATH_SELECTOR */
}

QString VBoxFilePathSelectorWidget::path() const
{
    return mPath;
}

void VBoxFilePathSelectorWidget::retranslateUi()
{
#ifdef VBOX_USE_COMBOBOX_PATH_SELECTOR
    mNoneStr = tr ("None");
    mCbPath->setItemText (SelectId, tr ("Other..."));
    mCbPath->setItemText (ResetId, tr ("Reset"));
#endif /* !VBOX_USE_COMBOBOX_PATH_SELECTOR */
}

void VBoxFilePathSelectorWidget::cbActivated (int aIndex)
{
#ifdef VBOX_USE_COMBOBOX_PATH_SELECTOR
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
   }
   mCbPath->setCurrentIndex (PathId);
#else /* VBOX_USE_COMBOBOX_PATH_SELECTOR */
   NOREF (aIndex);
#endif /* !VBOX_USE_COMBOBOX_PATH_SELECTOR */
}

void VBoxFilePathSelectorWidget::init()
{
    mIconProvider = new QFileIconProvider();
    QHBoxLayout *layout = new QHBoxLayout (this);
    VBoxGlobal::setLayoutMargin (layout, 0);
#ifdef VBOX_USE_COMBOBOX_PATH_SELECTOR
    mCbPath = new QComboBox();
    mCbPath->setMinimumWidth (200);
    mCbPath->insertItem (PathId, "");
    mCbPath->insertItem (SelectId, "");
    mCbPath->insertItem (ResetId, "");
    connect (mCbPath, SIGNAL (activated (int)),
             this, SLOT (cbActivated (int)));
    layout->addWidget (mCbPath);
#else /* VBOX_USE_COMBOBOX_PATH_SELECTOR */
    mLbIcon = new QLabel();
    mLePath = new QLineEdit();
    mLePath->setMinimumWidth (180);
    mLePath->setReadOnly (true);
    mTbSelect = new QToolButton();
    mTbSelect->setIcon (QIcon (":/select_file_16px.png"));
    mTbSelect->setAutoRaise (true);
    connect (mTbSelect, SIGNAL (clicked ()),
             this, SIGNAL (selectPath()));
    mTbReset = new QToolButton();
    mTbReset->setIcon (QIcon (":/eraser_16px.png"));
    mTbReset->setAutoRaise (true);
    connect (mTbReset, SIGNAL (clicked ()),
             this, SIGNAL (resetPath()));
    
    layout->addWidget (mLbIcon);
    layout->addWidget (mLePath);
    layout->addWidget (mTbSelect);
    layout->addWidget (mTbReset);
#endif /* !VBOX_USE_COMBOBOX_PATH_SELECTOR */

    retranslateUi();
}

QIcon VBoxFilePathSelectorWidget::defaultIcon() const
{
    if (mMode == PathMode)
        return mIconProvider->icon (QFileIconProvider::Folder);
    else
        return mIconProvider->icon (QFileIconProvider::File);
}  

QString VBoxFilePathSelectorWidget::filePath (const QString &aName, bool bLast) const
{
    if (mMode == PathMode)
    {
        QDir dir (aName);
        if (bLast)
            return dir.dirName();
        else
            return dir.path();
    }
    else
    {
        QFileInfo fi (aName);
        if (bLast)
            return fi.fileName();
        else
            return fi.filePath();
    }
}  

