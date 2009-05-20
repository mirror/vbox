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

#define ITEM_TYPE_ROLE Qt::UserRole + 1

/**
 *  Calculates a suitable page step size for the given max value. The returned
 *  size is so that there will be no more than 32 pages. The minimum returned
 *  page size is 4.
 */
static int calcPageStep (int aMax)
{
    /* reasonable max. number of page steps is 32 */
    uint page = ((uint) aMax + 31) / 32;
    /* make it a power of 2 */
    uint p = page, p2 = 0x1;
    while ((p >>= 1))
        p2 <<= 1;
    if (page != p2)
        p2 <<= 1;
    if (p2 < 4)
        p2 = 4;
    return (int) p2;
}

VBoxVMSettingsSystem::VBoxVMSettingsSystem()
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsSystem::setupUi (this);

    /* Setup constants */
    CSystemProperties sys = vboxGlobal().virtualBox().GetSystemProperties();
    const uint MinRAM = sys.GetMinGuestRAM();
    const uint MaxRAM = sys.GetMaxGuestRAM();
    const uint MinCPU = sys.GetMinGuestCPUCount();
    const uint MaxCPU = sys.GetMaxGuestCPUCount();

    /* Setup validators */
    mLeMemory->setValidator (new QIntValidator (MinRAM, MaxRAM, this));
    mLeCPU->setValidator (new QIntValidator (MinCPU, MaxCPU, this));

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

    /* Setup memory slider */
    mSlMemory->setPageStep (calcPageStep (MaxRAM));
    mSlMemory->setSingleStep (mSlMemory->pageStep() / 4);
    mSlMemory->setTickInterval (mSlMemory->pageStep());
    /* Setup the scale so that ticks are at page step boundaries */
    mSlMemory->setMinimum ((MinRAM / mSlMemory->pageStep()) * mSlMemory->pageStep());
    mSlMemory->setMaximum (MaxRAM);
    /* Limit min/max. size of QLineEdit */
    mLeMemory->setFixedWidthByText (QString().fill ('8', 5));
    /* Ensure mLeMemory value and validation is updated */
    valueChangedRAM (mSlMemory->value());

    /* Setup cpu slider */
    mSlCPU->setPageStep (1);
    mSlCPU->setSingleStep (1);
    mSlCPU->setTickInterval (1);
    /* Setup the scale so that ticks are at page step boundaries */
    mSlCPU->setMinimum (MinCPU);
    mSlCPU->setMaximum (MaxCPU);
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

    /* ACPI */
    mCbAcpi->setChecked (biosSettings.GetACPIEnabled());

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
    mCbVirt->setChecked (aMachine.GetHWVirtExEnabled() == KTSBool_True);

    /* Nested Paging */
    mCbNestedPaging->setEnabled (fVTxAMDVSupported &&
                                 aMachine.GetHWVirtExEnabled() == KTSBool_True);
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

    /* ACPI */
    biosSettings.SetACPIEnabled (mCbAcpi->isChecked());

    /* IO APIC */
    biosSettings.SetIOAPICEnabled (mCbApic->isChecked() ||
                                   mSlCPU->value() > 1);

    /* RAM size */
    mMachine.SetCPUCount (mSlCPU->value());

    /* PAE/NX */
    mMachine.SetPAEEnabled (mCbPae->isChecked());

    /* VT-x/AMD-V */
    mMachine.SetHWVirtExEnabled (mCbVirt->checkState() == Qt::Checked ||
                                 mSlCPU->value() > 1 ?
                                 KTSBool_True : KTSBool_False);

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
    /* Come up with some nice round percent boundraries relative to
     * the system memory. A max of 75% on a 256GB config is ridiculous,
     * even on an 8GB rig reserving 2GB for the OS is way to conservative.
     * The max numbers can be estimated using the following program:
     *
     *      double calcMaxPct(uint64_t cbRam)
     *      {
     *          double cbRamOverhead = cbRam * 0.0390625; // 160 bytes per page.
     *          double cbRamForTheOS = RT_MAX(RT_MIN(_512M, cbRam * 0.25), _64M);
     *          double OSPct  = (cbRamOverhead + cbRamForTheOS) * 100.0 / cbRam;
     *          double MaxPct = 100 - OSPct;
     *          return MaxPct;
     *      }
     *
     *      int main()
     *      {
     *          uint64_t cbRam = _1G;
     *          for (; !(cbRam >> 33); cbRam += _1G)
     *              printf("%8lluGB %.1f%% %8lluKB\n", cbRam >> 30, calcMaxPct(cbRam),
     *                     (uint64_t)(cbRam * calcMaxPct(cbRam) / 100.0) >> 20);
     *          for (; !(cbRam >> 51); cbRam <<= 1)
     *              printf("%8lluGB %.1f%% %8lluKB\n", cbRam >> 30, calcMaxPct(cbRam),
     *                     (uint64_t)(cbRam * calcMaxPct(cbRam) / 100.0) >> 20);
     *          return 0;
     *      }
     *
     * Note. We might wanna put these calculations somewhere global later. */

    /* System RAM amount test */
    ulong fullSize = vboxGlobal().virtualBox().GetHost().GetMemorySize();
    double maxPct  = 0.75;
    double warnPct = 0.50;
    if (fullSize < 3072)
        /* done */;
    else if (fullSize < 4096)   /* 3GB */
        maxPct  = 0.80;
    else if (fullSize < 6144)   /* 4-5GB */
    {
        maxPct  = 0.84;
        warnPct = 0.60;
    }
    else if (fullSize < 8192)   /* 6-7GB */
    {
        maxPct  = 0.88;
        warnPct = 0.65;
    }
    else if (fullSize < 16384)  /* 8-15GB */
    {
        maxPct  = 0.90;
        warnPct = 0.70;
    }
    else if (fullSize < 32768)  /* 16-31GB */
    {
        maxPct  = 0.93;
        warnPct = 0.75;
    }
    else if (fullSize < 65536)  /* 32-63GB */
    {
        maxPct  = 0.94;
        warnPct = 0.80;
    }
    else if (fullSize < 131072) /* 64-127GB */
    {
        maxPct  = 0.95;
        warnPct = 0.85;
    }
    else                        /* 128GB- */
    {
        maxPct  = 0.96;
        warnPct = 0.90;
    }

    if (mSlMemory->value() > maxPct * fullSize)
    {
        aWarning = tr (
            "you have assigned more than <b>%1%</b> of your computer's memory "
            "(<b>%2</b>) to the virtual machine. Not enough memory is left "
            "for your host operating system. Please select a smaller amount.")
            .arg ((unsigned)(maxPct * 100))
            .arg (vboxGlobal().formatSize ((uint64_t)fullSize * _1M));
        return false;
    }
    if (mSlMemory->value() > warnPct * fullSize)
    {
        aWarning = tr (
            "you have assigned more than <b>%1%</b> of your computer's memory "
            "(<b>%2</b>) to the virtual machine. Not enough memory might be "
            "left for your host operating system. Continue at your own risk.")
            .arg ((unsigned)(warnPct * 100))
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
            "there is more than one virtual CPU assigned for this VM, which "
            "requires IO-APIC feature to be enabled too, else SMP will not be "
            "able to work, so this feature will be enabled automatically when "
            "you'll accept VM Settings by pressing OK button.");
        return true;
    }

    /* VCPU VT-x/AMD-V test */
    if (mSlCPU->value() > 1 && !mCbVirt->isChecked())
    {
        aWarning = tr (
            "there is more than one virtual CPU assigned for this VM, which "
            "requires virtualization feature (VT-x/AMD-V) to be enabled too, "
            "else SMP will not be able to work, so this feature will be enabled "
            "automatically when you'll accept VM Settings by pressing OK button.");
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
    setTabOrder (mTbBootItemDown, mCbAcpi);
    setTabOrder (mCbAcpi, mCbApic);

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
    mLbMemoryMin->setText (tr ("<qt>%1&nbsp;MB</qt>").arg (sys.GetMinGuestRAM()));
    mLbMemoryMax->setText (tr ("<qt>%1&nbsp;MB</qt>").arg (sys.GetMaxGuestRAM()));

    /* Retranslate the cpu slider legend */
    mLbCPUMin->setText (tr ("<qt>%1&nbsp;CPU</qt>").arg (sys.GetMinGuestCPUCount()));
    mLbCPUMax->setText (tr ("<qt>%1&nbsp;CPU</qt>").arg (sys.GetMaxGuestCPUCount()));
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

    mTwBootOrder->setFixedSize (
        iv->sizeHintForColumn (0) + 2 * mTwBootOrder->frameWidth() + 4,
        iv->sizeHintForRow (0) * mTwBootOrder->topLevelItemCount() + 2 * mTwBootOrder->frameWidth());

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

