/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsSystem class implementation
 */

/*
 * Copyright (C) 2008-2009 Sun Microsystems, Inc.
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

#include "QIWidgetValidator.h"
#include "VBoxVMSettingsSystem.h"

#include <iprt/cdefs.h>

#define ITEM_TYPE_ROLE Qt::UserRole + 1

VBoxVMSettingsSystem::VBoxVMSettingsSystem()
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsSystem::setupUi (this);

    /* Setup constants */
    CSystemProperties sys = vboxGlobal().virtualBox().GetSystemProperties();
    uint hostCPUs = vboxGlobal().virtualBox().GetHost().GetProcessorCount();
    mMinGuestCPU = sys.GetMinGuestCPUCount();
    mMaxGuestCPU = RT_MIN (2 * hostCPUs, sys.GetMaxGuestCPUCount());

    /* Setup validators */
    mLeMemory->setValidator (new QIntValidator (mSlMemory->minRAM(), mSlMemory->maxRAM(), this));
    mLeCPU->setValidator (new QIntValidator (mMinGuestCPU, mMaxGuestCPU, this));

    /* Setup connections */
    connect (mSlMemory, SIGNAL (valueChanged (int)),
             this, SLOT (valueChangedRAM (int)));
    connect (mLeMemory, SIGNAL (textChanged (const QString&)),
             this, SLOT (textChangedRAM (const QString&)));

    connect (mTbBootItemUp, SIGNAL (clicked()),
             this, SLOT (moveBootItemUp()));
    connect (mTbBootItemDown, SIGNAL (clicked()),
             this, SLOT (moveBootItemDown()));
    connect (mTwBootOrder, SIGNAL (moveItemUp()),
             this, SLOT (moveBootItemUp()));
    connect (mTwBootOrder, SIGNAL (moveItemDown()),
             this, SLOT (moveBootItemDown()));
    connect (mTwBootOrder, SIGNAL (itemToggled()),
             this, SIGNAL (tableChanged()));
    connect (mTwBootOrder, SIGNAL (currentItemChanged (QTreeWidgetItem*,
                                                       QTreeWidgetItem*)),
             this, SLOT (onCurrentBootItemChanged (QTreeWidgetItem*,
                                                   QTreeWidgetItem*)));

    connect (mSlCPU, SIGNAL (valueChanged (int)),
             this, SLOT (valueChangedCPU (int)));
    connect (mLeCPU, SIGNAL (textChanged (const QString&)),
             this, SLOT (textChangedCPU (const QString&)));

    /* Setup iconsets */
    mTbBootItemUp->setIcon (VBoxGlobal::iconSet (":/list_moveup_16px.png",
                                                 ":/list_moveup_disabled_16px.png"));
    mTbBootItemDown->setIcon (VBoxGlobal::iconSet (":/list_movedown_16px.png",
                                                   ":/list_movedown_disabled_16px.png"));

#ifdef Q_WS_MAC
    /* We need a little space for the focus rect. */
    mLtBootOrder->setContentsMargins (3, 3, 3, 3);
    mLtBootOrder->setSpacing (3);
#endif /* Q_WS_MAC */

    /* Limit min/max. size of QLineEdit */
    mLeMemory->setFixedWidthByText (QString().fill ('8', 5));
    /* Ensure mLeMemory value and validation is updated */
    valueChangedRAM (mSlMemory->value());

    /* Setup cpu slider */
    mSlCPU->setPageStep (1);
    mSlCPU->setSingleStep (1);
    mSlCPU->setTickInterval (1);
    /* Setup the scale so that ticks are at page step boundaries */
    mSlCPU->setMinimum (mMinGuestCPU);
    mSlCPU->setMaximum (mMaxGuestCPU);
    mSlCPU->setOptimalHint (1, hostCPUs);
    mSlCPU->setWarningHint (hostCPUs, mMaxGuestCPU);
    /* Limit min/max. size of QLineEdit */
    mLeCPU->setFixedWidthByText (QString().fill ('8', 3));
    /* Ensure mLeMemory value and validation is updated */
    valueChangedCPU (mSlCPU->value());

    /* Install global event filter */
    qApp->installEventFilter (this);

    /* Applying language settings */
    retranslateUi();
}

bool VBoxVMSettingsSystem::isHWVirtExEnabled() const
{
    return mCbVirt->isChecked();
}

int VBoxVMSettingsSystem::cpuCount() const
{
    return mSlCPU->value();
}

void VBoxVMSettingsSystem::getFrom (const CMachine &aMachine)
{
    mMachine = aMachine;
    CBIOSSettings biosSettings = mMachine.GetBIOSSettings();

    /* RAM size */
    mSlMemory->setValue (aMachine.GetMemorySize());

    /* Boot-order */
    {
        mTwBootOrder->clear();
        /* Load boot-items of current VM */
        QStringList uniqueList;
        for (int i = 1; i <= 4; ++ i)
        {
            KDeviceType type = mMachine.GetBootOrder (i);
            if (type != KDeviceType_Null)
            {
                QString name = vboxGlobal().toString (type);
                QTreeWidgetItem *item =
                    new QTreeWidgetItem (mTwBootOrder, QStringList (name));
                QVariant vtype (type);
                item->setData (0, ITEM_TYPE_ROLE, vtype);
                item->setCheckState (0, Qt::Checked);
                uniqueList << name;
            }
        }
        /* Load other unique boot-items */
        for (int i = KDeviceType_Floppy; i < KDeviceType_USB; ++ i)
        {
            QString name = vboxGlobal().toString ((KDeviceType) i);
            if (!uniqueList.contains (name))
            {
                QTreeWidgetItem *item =
                    new QTreeWidgetItem (mTwBootOrder, QStringList (name));
                item->setData (0, ITEM_TYPE_ROLE, i);
                item->setCheckState (0, Qt::Unchecked);
                uniqueList << name;
            }
        }
        adjustBootOrderTWSize();
    }

    /* IO APIC */
    mCbApic->setChecked (biosSettings.GetIOAPICEnabled());

    /* CPU count */
    bool fVTxAMDVSupported = vboxGlobal().virtualBox().GetHost()
                             .GetProcessorFeature (KProcessorFeature_HWVirtEx);
    mSlCPU->setEnabled (fVTxAMDVSupported);
    mLeCPU->setEnabled (fVTxAMDVSupported);
    mSlCPU->setValue (fVTxAMDVSupported ? aMachine.GetCPUCount() : 1);

    /* PAE/NX */
    bool fPAESupported = vboxGlobal().virtualBox().GetHost()
                         .GetProcessorFeature (KProcessorFeature_PAE);
    mCbPae->setEnabled (fPAESupported);
    mCbPae->setChecked (aMachine.GetPAEEnabled());

    /* VT-x/AMD-V */
    mCbVirt->setEnabled (fVTxAMDVSupported);
    mCbVirt->setChecked (aMachine.GetHWVirtExEnabled());

    /* Nested Paging */
    mCbNestedPaging->setEnabled (fVTxAMDVSupported &&
                                 aMachine.GetHWVirtExEnabled());
    mCbNestedPaging->setChecked (aMachine.GetHWVirtExNestedPagingEnabled());

    if (mValidator)
        mValidator->revalidate();
}

void VBoxVMSettingsSystem::putBackTo()
{
    CBIOSSettings biosSettings = mMachine.GetBIOSSettings();

    /* RAM size */
    mMachine.SetMemorySize (mSlMemory->value());

    /* Boot order */
    {
        /* Search for checked items */
        int index = 1;

        for (int i = 0; i < mTwBootOrder->topLevelItemCount(); ++ i)
        {
            QTreeWidgetItem *item = mTwBootOrder->topLevelItem (i);
            if (item->checkState (0) == Qt::Checked)
            {
                KDeviceType type = vboxGlobal().toDeviceType (item->text (0));
                mMachine.SetBootOrder (index ++, type);
            }
        }

        /* Search for non-checked items */
        for (int i = 0; i < mTwBootOrder->topLevelItemCount(); ++ i)
        {
            QTreeWidgetItem *item = mTwBootOrder->topLevelItem (i);
            if (item->checkState (0) != Qt::Checked)
                mMachine.SetBootOrder (index ++, KDeviceType_Null);
        }
    }

    /* IO APIC */
    biosSettings.SetIOAPICEnabled (mCbApic->isChecked() ||
                                   mSlCPU->value() > 1);

    /* RAM size */
    mMachine.SetCPUCount (mSlCPU->value());

    /* PAE/NX */
    mMachine.SetPAEEnabled (mCbPae->isChecked());

    /* VT-x/AMD-V */
    mMachine.SetHWVirtExEnabled (mCbVirt->checkState() == Qt::Checked ||
                                 mSlCPU->value() > 1);

    /* Nested Paging */
    mMachine.SetHWVirtExNestedPagingEnabled (mCbNestedPaging->isChecked());
}

void VBoxVMSettingsSystem::setValidator (QIWidgetValidator *aVal)
{
    mValidator = aVal;
    connect (mCbApic, SIGNAL (stateChanged (int)), mValidator, SLOT (revalidate()));
    connect (mCbVirt, SIGNAL (stateChanged (int)), mValidator, SLOT (revalidate()));
}

bool VBoxVMSettingsSystem::revalidate (QString &aWarning, QString & /* aTitle */)
{
    ulong fullSize = vboxGlobal().virtualBox().GetHost().GetMemorySize();
    if (mSlMemory->value() > (int)mSlMemory->maxRAMAlw())
    {
        aWarning = tr (
            "you have assigned more than <b>%1%</b> of your computer's memory "
            "(<b>%2</b>) to the virtual machine. Not enough memory is left "
            "for your host operating system. Please select a smaller amount.")
            .arg ((unsigned)qRound ((double)mSlMemory->maxRAMAlw() / fullSize * 100.0))
            .arg (vboxGlobal().formatSize ((uint64_t)fullSize * _1M));
        return false;
    }
    if (mSlMemory->value() > (int)mSlMemory->maxRAMOpt())
    {
        aWarning = tr (
            "you have assigned more than <b>%1%</b> of your computer's memory "
            "(<b>%2</b>) to the virtual machine. Not enough memory might be "
            "left for your host operating system. Continue at your own risk.")
            .arg ((unsigned)qRound ((double)mSlMemory->maxRAMOpt() / fullSize * 100.0))
            .arg (vboxGlobal().formatSize ((uint64_t)fullSize * _1M));
        return true;
    }

    /* VCPU amount test */
    int totalCPUs = vboxGlobal().virtualBox().GetHost().GetProcessorOnlineCount();
    if (mSlCPU->value() > 2 * totalCPUs)
    {
        aWarning = tr (
            "for performance reasons, the number of virtual CPUs attached to the "
            "virtual machine may not be more than twice the number of physical "
            "CPUs on the host (<b>%1</b>). Please reduce the number of virtual CPUs.")
            .arg (totalCPUs);
        return false;
    }
    if (mSlCPU->value() > totalCPUs)
    {
        aWarning = tr (
            "you have assigned more virtual CPUs to the virtual machine than "
            "the number of physical CPUs on your host system (<b>%1</b>). "
            "This is likely to degrade the performance of your virtual machine. "
            "Please consider reducing the number of virtual CPUs.")
            .arg (totalCPUs);
        return true;
    }

    /* VCPU IO-APIC test */
    if (mSlCPU->value() > 1 && !mCbApic->isChecked())
    {
        aWarning = tr (
            "you have assigned more than one virtual CPU to this VM. "
            "This will not work unless the IO-APIC feature is also enabled. "
            "This will be done automatically when you accept the VM Settings "
            "by pressing the OK button.");
        return true;
    }

    /* VCPU VT-x/AMD-V test */
    if (mSlCPU->value() > 1 && !mCbVirt->isChecked())
    {
        aWarning = tr (
            "you have assigned more than one virtual CPU to this VM. "
            "This will not work unless hardware virtualization (VT-x/AMD-V) is also enabled. "
            "This will be done automatically when you accept the VM Settings "
            "by pressing the OK button.");
        return true;
    }

    return true;
}

void VBoxVMSettingsSystem::setOrderAfter (QWidget *aWidget)
{
    setTabOrder (aWidget, mTwSystem->focusProxy());
    setTabOrder (mTwSystem->focusProxy(), mSlMemory);
    setTabOrder (mSlMemory, mLeMemory);
    setTabOrder (mLeMemory, mTwBootOrder);
    setTabOrder (mTwBootOrder, mTbBootItemUp);
    setTabOrder (mTbBootItemUp, mTbBootItemDown);
    setTabOrder (mTbBootItemDown, mCbApic);

    setTabOrder (mCbApic, mSlCPU);
    setTabOrder (mSlCPU, mLeCPU);
    setTabOrder (mLeCPU, mCbPae);

    setTabOrder (mCbPae, mCbVirt);
    setTabOrder (mCbVirt, mCbNestedPaging);
}

void VBoxVMSettingsSystem::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsSystem::retranslateUi (this);

    /* Adjust the boot order tree widget */
    mTwBootOrder->header()->setResizeMode (QHeaderView::ResizeToContents);
    mTwBootOrder->resizeColumnToContents (0);
    mTwBootOrder->updateGeometry();
    /* Retranslate the boot order items */
    QTreeWidgetItemIterator it (mTwBootOrder);
    while (*it)
    {
        QTreeWidgetItem *item = (*it);
        item->setText (0, vboxGlobal().toString (
             static_cast <KDeviceType> (item->data (0, ITEM_TYPE_ROLE).toInt())));
        ++ it;
    }
    /* Readjust the tree widget items size */
    adjustBootOrderTWSize();

    CSystemProperties sys = vboxGlobal().virtualBox().GetSystemProperties();

    /* Retranslate the memory slider legend */
    mLbMemoryMin->setText (tr ("<qt>%1&nbsp;MB</qt>").arg (mSlMemory->minRAM()));
    mLbMemoryMax->setText (tr ("<qt>%1&nbsp;MB</qt>").arg (mSlMemory->maxRAM()));

    /* Retranslate the cpu slider legend */
    mLbCPUMin->setText (tr ("<qt>%1&nbsp;CPU</qt>", "%1 is 1 for now").arg (mMinGuestCPU));
    mLbCPUMax->setText (tr ("<qt>%1&nbsp;CPUs</qt>", "%1 is host cpu count * 2 for now").arg (mMaxGuestCPU));
}

void VBoxVMSettingsSystem::valueChangedRAM (int aVal)
{
    mLeMemory->setText (QString().setNum (aVal));
}

void VBoxVMSettingsSystem::textChangedRAM (const QString &aText)
{
    mSlMemory->setValue (aText.toInt());
}

void VBoxVMSettingsSystem::moveBootItemUp()
{
    QTreeWidgetItem *item = mTwBootOrder->currentItem();
    Assert (item);
    if (!mTwBootOrder->itemAbove (item))
        return;

    int index = mTwBootOrder->indexOfTopLevelItem (item);
    QTreeWidgetItem *takenItem = mTwBootOrder->takeTopLevelItem (index);
    Assert (takenItem == item);

    mTwBootOrder->insertTopLevelItem (index - 1, takenItem);
    mTwBootOrder->setCurrentItem (item);

    emit tableChanged();
}

void VBoxVMSettingsSystem::moveBootItemDown()
{
    QTreeWidgetItem *item = mTwBootOrder->currentItem();
    Assert (item);
    if (!mTwBootOrder->itemBelow (item))
        return;

    int index = mTwBootOrder->indexOfTopLevelItem (item);
    QTreeWidgetItem *takenItem = mTwBootOrder->takeTopLevelItem (index);
    Assert (takenItem == item);

    mTwBootOrder->insertTopLevelItem (index + 1, takenItem);
    mTwBootOrder->setCurrentItem (item);

    emit tableChanged();
}

void VBoxVMSettingsSystem::onCurrentBootItemChanged (QTreeWidgetItem *aItem,
                                                     QTreeWidgetItem *)
{
    bool upEnabled   = aItem && mTwBootOrder->itemAbove (aItem);
    bool downEnabled = aItem && mTwBootOrder->itemBelow (aItem);
    if ((mTbBootItemUp->hasFocus() && !upEnabled) ||
        (mTbBootItemDown->hasFocus() && !downEnabled))
        mTwBootOrder->setFocus();
    mTbBootItemUp->setEnabled (upEnabled);
    mTbBootItemDown->setEnabled (downEnabled);
}

void VBoxVMSettingsSystem::adjustBootOrderTWSize()
{
    /* Calculate the optimal size of the tree widget & set it as fixed
     * size. */

    QAbstractItemView *iv = qobject_cast <QAbstractItemView*> (mTwBootOrder);

    int h = 2 * mTwBootOrder->frameWidth();
    int w = h;
#ifdef Q_WS_MAC
    int left, top, right, bottom;
    mTwBootOrder->getContentsMargins (&left, &top, &right, &bottom);
    h += top + bottom;
    w += left + right;
#else /* Q_WS_MAC */
    w += 4;
#endif /* Q_WS_MAC */
    mTwBootOrder->setFixedSize (
        iv->sizeHintForColumn (0) + w,
        iv->sizeHintForRow (0) * mTwBootOrder->topLevelItemCount() + h);

    /* Update the layout system */
    if (mTabMotherboard->layout())
    {
        mTabMotherboard->layout()->activate();
        mTabMotherboard->layout()->update();
    }
}

void VBoxVMSettingsSystem::valueChangedCPU (int aVal)
{
    mLeCPU->setText (QString().setNum (aVal));
}

void VBoxVMSettingsSystem::textChangedCPU (const QString &aText)
{
    mSlCPU->setValue (aText.toInt());
}

bool VBoxVMSettingsSystem::eventFilter (QObject *aObject, QEvent *aEvent)
{
    if (!aObject->isWidgetType())
        return QWidget::eventFilter (aObject, aEvent);

    QWidget *widget = static_cast<QWidget*> (aObject);
    if (widget->window() != window())
        return QWidget::eventFilter (aObject, aEvent);

    switch (aEvent->type())
    {
        case QEvent::FocusIn:
        {
            /* Boot Table */
            if (widget == mTwBootOrder)
            {
                if (!mTwBootOrder->currentItem())
                    mTwBootOrder->setCurrentItem (mTwBootOrder->topLevelItem (0));
                else
                    onCurrentBootItemChanged (mTwBootOrder->currentItem());
                mTwBootOrder->currentItem()->setSelected (true);
            }
            else if (widget != mTbBootItemUp && widget != mTbBootItemDown)
            {
                if (mTwBootOrder->currentItem())
                {
                    mTwBootOrder->currentItem()->setSelected (false);
                    mTbBootItemUp->setEnabled (false);
                    mTbBootItemDown->setEnabled (false);
                }
            }
            break;
        }
        default:
            break;
    }

    return QWidget::eventFilter (aObject, aEvent);
}

