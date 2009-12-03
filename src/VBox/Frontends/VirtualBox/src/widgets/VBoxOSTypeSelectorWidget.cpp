/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxOSTypeSelectorWidget class implementation
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

#include "VBoxOSTypeSelectorWidget.h"

#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>

enum
{
    RoleTypeID = Qt::UserRole + 1
};

VBoxOSTypeSelectorWidget::VBoxOSTypeSelectorWidget (QWidget *aParent)
    : QIWithRetranslateUI <QWidget> (aParent)
    , mTxFamilyName (new QLabel (this))
    , mTxTypeName (new QLabel (this))
    , mPxTypeIcon (new QLabel (this))
    , mCbFamily (new QComboBox (this))
    , mCbType (new QComboBox (this))
    , mPolished (false)
    , mLayoutPosition (-1)
{
    /* Setup widgets */
    mTxFamilyName->setAlignment (Qt::AlignRight);
    mTxTypeName->setAlignment (Qt::AlignRight);
    mTxFamilyName->setBuddy (mCbFamily);
    mTxTypeName->setBuddy (mCbType);
    mTxFamilyName->setSizePolicy (QSizePolicy::Minimum, QSizePolicy::Fixed);
    mTxTypeName->setSizePolicy (QSizePolicy::Minimum, QSizePolicy::Fixed);
    mCbFamily->setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Fixed);
    mCbType->setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Fixed);
    mPxTypeIcon->setFixedSize (32, 32);

    /* Fill OS family selector */
    int maximumSize = 0;
    QFontMetrics fm (mCbFamily->font());
    QList <CGuestOSType> families (vboxGlobal().vmGuestOSFamilyList());
    for (int i = 0; i < families.size(); ++ i)
    {
        /* Search for maximum length among family names */
        QString familyName (families [i].GetFamilyDescription());
        maximumSize = maximumSize < fm.width (familyName) ?
                      fm.width (familyName) : maximumSize;
        mCbFamily->insertItem (i, familyName);
        mCbFamily->setItemData (i, families [i].GetFamilyId(), RoleTypeID);
        /* Search for maximum length among type names */
        QList <CGuestOSType> types (vboxGlobal().vmGuestOSTypeList (families [i].GetFamilyId()));
        for (int j = 0; j < types.size(); ++ j)
        {
            QString typeName (types [j].GetDescription());
            maximumSize = maximumSize < fm.width (typeName) ?
                          fm.width (typeName) : maximumSize;
        }
    }
    mCbFamily->setCurrentIndex (0);
    onFamilyChanged (mCbFamily->currentIndex());

    /* Set the minimum size for OS Type & Family selectors. */
    QStyleOptionComboBox options;
    options.initFrom (mCbFamily);
    QSize size (style()->sizeFromContents (QStyle::CT_ComboBox, &options,
                QSize (maximumSize, fm.height()), mCbFamily));
    mCbFamily->setMinimumWidth (size.width());
    mCbType->setMinimumWidth (size.width());

    /* Slots connections */
    connect (mCbFamily, SIGNAL (currentIndexChanged (int)),
             this, SLOT (onFamilyChanged (int)));
    connect (mCbType, SIGNAL (currentIndexChanged (int)),
             this, SLOT (onTypeChanged (int)));

    /* Retranslate */
    retranslateUi();
}

void VBoxOSTypeSelectorWidget::setType (const CGuestOSType &aType)
{
    QString familyId (aType.GetFamilyId());
    QString typeId (aType.GetId());

    int familyIndex = mCbFamily->findData (familyId, RoleTypeID);
    AssertMsg (familyIndex != -1, ("Family ID should be valid: '%s'", familyId.toLatin1().constData()));
    if (familyIndex != -1)
        mCbFamily->setCurrentIndex (familyIndex);

    int typeIndex = mCbType->findData (typeId, RoleTypeID);
    AssertMsg (typeIndex != -1, ("Type ID should be valid: '%s'", typeId.toLatin1().constData()));
    if (typeIndex != -1)
        mCbType->setCurrentIndex (typeIndex);
}

CGuestOSType VBoxOSTypeSelectorWidget::type() const
{
    return mType;
}

void VBoxOSTypeSelectorWidget::setLayoutPosition (int aPos)
{
    mLayoutPosition = aPos;
}

void VBoxOSTypeSelectorWidget::retranslateUi()
{
    mTxFamilyName->setText (tr ("Operating &System:"));
    mCbFamily->setWhatsThis (tr ("Displays the operating system family that "
                                 "you plan to install into this virtual machine."));
    mTxTypeName->setText (tr ("&Version:"));
    mCbType->setWhatsThis (tr ("Displays the operating system type that "
                               "you plan to install into this virtual "
                               "machine (called a guest operating system)."));
}

void VBoxOSTypeSelectorWidget::showEvent (QShowEvent *aEvent)
{
    if (!mPolished)
    {
        /* Finally polishing just before first show event */
        mPolished = true;

        /* Layouting widgets */
        QVBoxLayout *layout1 = new QVBoxLayout();
        layout1->setSpacing (0);
        layout1->addWidget (mPxTypeIcon);
        layout1->addStretch();

        QGridLayout *layout2 = layout() ? static_cast <QGridLayout*> (layout()) :
                                          new QGridLayout (this);
        layout2->setMargin (0);
        int row = mLayoutPosition == -1 ? layout2->rowCount() : mLayoutPosition;
        layout2->addWidget (mTxFamilyName, row, 0);
        layout2->addWidget (mCbFamily, row, 1);
        layout2->addWidget (mTxTypeName, row + 1, 0);
        layout2->addWidget (mCbType, row + 1, 1);
        layout2->addLayout (layout1, row, 2, 2, 1);
    }
    QIWithRetranslateUI <QWidget>::showEvent (aEvent);
}

void VBoxOSTypeSelectorWidget::onFamilyChanged (int aIndex)
{
    /* Lock the signals of mCbType to prevent it's reaction on clearing */
    mCbType->blockSignals (true);
    mCbType->clear();

    /* Check if host supports (AMD-V or VT-x) and long mode */
    CHost host = vboxGlobal().virtualBox().GetHost();
    bool mSupportsHWVirtEx = host.GetProcessorFeature (KProcessorFeature_HWVirtEx);
    bool mSupportsLongMode = host.GetProcessorFeature (KProcessorFeature_LongMode);

    /* Populate combo-box with OS Types related to currently selected Family ID */
    QString familyId (mCbFamily->itemData (aIndex, RoleTypeID).toString());
    QList <CGuestOSType> types (vboxGlobal().vmGuestOSTypeList (familyId));
    for (int i = 0; i < types.size(); ++ i)
    {
        if (types [i].GetIs64Bit() && (!mSupportsHWVirtEx || !mSupportsLongMode))
            continue;
        int index = mCbType->count();
        mCbType->insertItem (index, types [i].GetDescription());
        mCbType->setItemData (index, types [i].GetId(), RoleTypeID);
    }

    /* Select the most recently chosen item */
    if (mCurrentIds.contains (familyId))
    {
        QString typeId (mCurrentIds [familyId]);
        int typeIndex = mCbType->findData (typeId, RoleTypeID);
        if (typeIndex != -1)
            mCbType->setCurrentIndex (typeIndex);
    }
    /* Or select WinXP item for Windows family as default */
    else if (familyId == "Windows")
    {
        int xpIndex = mCbType->findData ("WindowsXP", RoleTypeID);
        if (xpIndex != -1)
            mCbType->setCurrentIndex (xpIndex);
    }
    /* Or select Ubuntu item for Linux family as default */
    else if (familyId == "Linux")
    {
        int ubIndex = mCbType->findData ("Ubuntu", RoleTypeID);
        if (ubIndex != -1)
            mCbType->setCurrentIndex (ubIndex);
    }
    /* Else simply select the first one present */
    else mCbType->setCurrentIndex (0);

    /* Update all the stuff with new type */
    onTypeChanged (mCbType->currentIndex());

    /* Unlock the signals of mCbType */
    mCbType->blockSignals (false);
}

void VBoxOSTypeSelectorWidget::onTypeChanged (int aIndex)
{
    /* Save the new selected OS Type */
    mType = vboxGlobal().vmGuestOSType (
        mCbType->itemData (aIndex, RoleTypeID).toString(),
        mCbFamily->itemData (mCbFamily->currentIndex(), RoleTypeID).toString());
    mPxTypeIcon->setPixmap (vboxGlobal().vmGuestOSTypeIcon (mType.GetId()));

    /* Save the most recently used item */
    mCurrentIds [mType.GetFamilyId()] = mType.GetId();

    /* Notify connected objects about type was changed */
    emit osTypeChanged();
}

