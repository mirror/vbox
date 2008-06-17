/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsGeneral class implementation
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#include "VBoxVMSettingsGeneral.h"
#include "VBoxVMSettingsDlg.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "QIWidgetValidator.h"

/* Qt includes */
#include <QDir>

#define ITEM_TYPE_ROLE Qt::UserRole + 1

VBoxVMSettingsGeneral* VBoxVMSettingsGeneral::mSettings = 0;

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

VBoxVMSettingsGeneral::VBoxVMSettingsGeneral (QWidget *aParent,
                                              VBoxVMSettingsDlg *aDlg,
                                              const QString &aPath)
    : QIWithRetranslateUI<QWidget> (aParent)
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsGeneral::setupUi (this);

    /* Setup validators */
    CSystemProperties sys = vboxGlobal().virtualBox().GetSystemProperties();
    const uint MinRAM = sys.GetMinGuestRAM();
    const uint MaxRAM = sys.GetMaxGuestRAM();
    const uint MinVRAM = sys.GetMinGuestVRAM();
    const uint MaxVRAM = sys.GetMaxGuestVRAM();

    mLeName->setValidator (new QRegExpValidator (QRegExp (".+"), aDlg));
    mLeRam->setValidator (new QIntValidator (MinRAM, MaxRAM, aDlg));
    mLeVideo->setValidator (new QIntValidator (MinVRAM, MaxVRAM, aDlg));

    mValidator = new QIWidgetValidator (aPath, aParent, aDlg);
    connect (mValidator, SIGNAL (validityChanged (const QIWidgetValidator *)),
             aDlg, SLOT (enableOk (const QIWidgetValidator *)));

    /* Setup connections */
    connect (mSlRam, SIGNAL (valueChanged (int)),
             this, SLOT (valueChangedRAM (int)));
    connect (mSlVideo, SIGNAL (valueChanged (int)),
             this, SLOT (valueChangedVRAM (int)));
    connect (mLeRam, SIGNAL (textChanged (const QString&)),
             this, SLOT (textChangedRAM (const QString&)));
    connect (mLeVideo, SIGNAL (textChanged (const QString&)),
             this, SLOT (textChangedVRAM (const QString&)));
    connect (mTbSelectSnapshot, SIGNAL (clicked()),
             this, SLOT (selectSnapshotFolder()));
    connect (mTbResetSnapshot, SIGNAL (clicked()),
             this, SLOT (resetSnapshotFolder()));
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

    /* Setup iconsets */
    mTbBootItemUp->setIcon (VBoxGlobal::iconSet (":/list_moveup_16px.png",
                                                 ":/list_moveup_disabled_16px.png"));
    mTbBootItemDown->setIcon (VBoxGlobal::iconSet (":/list_movedown_16px.png",
                                                   ":/list_movedown_disabled_16px.png"));

    /* Setup initial values */

    mCbOsType->insertItems (0, vboxGlobal().vmGuestOSTypeDescriptions());

    mSlRam->setPageStep (calcPageStep (MaxRAM));
    mSlRam->setSingleStep (mSlRam->pageStep() / 4);
    mSlRam->setTickInterval (mSlRam->pageStep());
    /* Setup the scale so that ticks are at page step boundaries */
    mSlRam->setMinimum ((MinRAM / mSlRam->pageStep()) * mSlRam->pageStep());
    mSlRam->setMaximum (MaxRAM);
    /* Limit min/max. size of QLineEdit */
    mLeRam->setMaximumSize (mLeRam->fontMetrics().width ("99999"),
                            mLeRam->minimumSizeHint().height());
    mLeRam->setMinimumSize (mLeRam->maximumSize());
    /* Ensure mLeRam value and validation is updated */
    valueChangedRAM (mSlRam->value());

    mSlVideo->setPageStep (calcPageStep (MaxVRAM));
    mSlVideo->setSingleStep (mSlVideo->pageStep() / 4);
    mSlVideo->setTickInterval (mSlVideo->pageStep());
    /* Setup the scale so that ticks are at page step boundaries */
    mSlVideo->setMinimum ((MinVRAM / mSlVideo->pageStep()) * mSlVideo->pageStep());
    mSlVideo->setMaximum (MaxVRAM);
    /* Limit min/max. size of QLineEdit */
    mLeVideo->setMaximumSize (mLeVideo->fontMetrics().width ("99999"),
                              mLeVideo->minimumSizeHint().height());
    mLeVideo->setMinimumSize (mLeVideo->maximumSize());
    /* Ensure mLeVideo value and validation is updated */
    valueChangedVRAM (mSlVideo->value());

    /* Shared Clipboard mode */
    mCbClipboard->addItem (""); /* KClipboardMode_Disabled */
    mCbClipboard->addItem (""); /* KClipboardMode_HostToGuest */
    mCbClipboard->addItem (""); /* KClipboardMode_GuestToHost */
    mCbClipboard->addItem (""); /* KClipboardMode_Bidirectional */

    /* IDE Controller Type */
    mCbIDEController->addItem (""); /* KIDEControllerType_PIIX3 */
    mCbIDEController->addItem (""); /* KIDEControllerType_PIIX4 */

    qApp->installEventFilter (this);
    /* Applying language settings */
    retranslateUi();
}

void VBoxVMSettingsGeneral::getFromMachine (const CMachine &aMachine,
                                            QWidget *aPage,
                                            VBoxVMSettingsDlg *aDlg,
                                            const QString &aPath)
{
    mSettings = new VBoxVMSettingsGeneral (aPage, aDlg, aPath);
    QVBoxLayout *layout = new QVBoxLayout (aPage);
    layout->setContentsMargins (0, 0, 0, 0);
    layout->addWidget (mSettings);
    connect (mSettings, SIGNAL (tableChanged()), aDlg, SLOT (resetFirstRunFlag()));
    mSettings->getFrom (aMachine);
}

void VBoxVMSettingsGeneral::putBackToMachine()
{
    mSettings->putBackTo();
}

void VBoxVMSettingsGeneral::getFrom (const CMachine &aMachine)
{
    mMachine = aMachine;
    CBIOSSettings biosSettings = mMachine.GetBIOSSettings();

    /* Name */
    mLeName->setText (aMachine.GetName());

    /* OS type */
    QString typeId = aMachine.GetOSTypeId();
    mCbOsType->setCurrentIndex (vboxGlobal().vmGuestOSTypeIndex (typeId));

    /* RAM size */
    mSlRam->setValue (aMachine.GetMemorySize());

    /* VRAM size */
    mSlVideo->setValue (aMachine.GetVRAMSize());

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
        adjustBootOrderTWSize ();
    }

    /* ACPI */
    mCbAcpi->setChecked (biosSettings.GetACPIEnabled());

    /* IO APIC */
    mCbApic->setChecked (biosSettings.GetIOAPICEnabled());

    /* VT-x/AMD-V */
    aMachine.GetHWVirtExEnabled() == KTSBool_False ?
        mCbVirt->setCheckState (Qt::Unchecked) :
    aMachine.GetHWVirtExEnabled() == KTSBool_True ?
        mCbVirt->setChecked (Qt::Checked) :
        mCbVirt->setChecked (Qt::PartiallyChecked);

    /* PAE/NX */
    mCbPae->setChecked (aMachine.GetPAEEnabled());

    /* Snapshot folder */
    mLeSnapshot->setText (aMachine.GetSnapshotFolder());

    /* Description */
    mTeDescription->setPlainText (aMachine.GetDescription());

    /* Shared clipboard mode */
    mCbClipboard->setCurrentIndex (aMachine.GetClipboardMode());

    /* IDE controller type */
    mCbIDEController->setCurrentIndex (mCbIDEController->
        findText (vboxGlobal().toString (biosSettings.GetIDEControllerType())));

    /* Other features */
    QString saveRtimeImages = mMachine.GetExtraData (VBoxDefs::GUI_SaveMountedAtRuntime);
    mCbSaveMounted->setChecked (saveRtimeImages != "no");
}

void VBoxVMSettingsGeneral::putBackTo()
{
    CBIOSSettings biosSettings = mMachine.GetBIOSSettings();

    /* Name */
    mMachine.SetName (mLeName->text());

    /* OS type */
    CGuestOSType type = vboxGlobal().vmGuestOSType (mCbOsType->currentIndex());
    AssertMsg (!type.isNull(), ("vmGuestOSType() must return non-null type"));
    mMachine.SetOSTypeId (type.GetId());

    /* RAM size */
    mMachine.SetMemorySize (mSlRam->value());

    /* VRAM size */
    mMachine.SetVRAMSize (mSlVideo->value());

    /* boot order */
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
    biosSettings.SetIOAPICEnabled (mCbApic->isChecked());

    /* VT-x/AMD-V */
    mMachine.SetHWVirtExEnabled (
        mCbVirt->checkState() == Qt::Unchecked ? KTSBool_False :
        mCbVirt->checkState() == Qt::Checked ? KTSBool_True : KTSBool_Default);

    /* PAE/NX */
    mMachine.SetPAEEnabled (mCbPae->isChecked());

    /* Saved state folder */
    if (mLeSnapshot->isModified())
    {
        mMachine.SetSnapshotFolder (mLeSnapshot->text());
        if (!mMachine.isOk())
            vboxProblem().cannotSetSnapshotFolder (mMachine,
                    QDir::convertSeparators (mLeSnapshot->text()));
    }

    /* Description (set empty to null to avoid an empty <Description> node
     * in the settings file) */
    mMachine.SetDescription (mTeDescription->toPlainText().isEmpty() ?
                             QString::null : mTeDescription->toPlainText());

    /* Shared clipboard mode */
    mMachine.SetClipboardMode ((KClipboardMode) mCbClipboard->currentIndex());

    /* IDE controller type */
    biosSettings.SetIDEControllerType (vboxGlobal().toIDEControllerType (mCbIDEController->currentText()));

    /* Other features */
    mMachine.SetExtraData (VBoxDefs::GUI_SaveMountedAtRuntime,
                           mCbSaveMounted->isChecked() ? "yes" : "no");
}


void VBoxVMSettingsGeneral::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsGeneral::retranslateUi (this);
    mTwBootOrder->header()->setResizeMode (QHeaderView::ResizeToContents);
    mTwBootOrder->resizeColumnToContents (0);
    mTwBootOrder->updateGeometry();

    CSystemProperties sys = vboxGlobal().virtualBox().GetSystemProperties();
    mLbRamMin->setText (tr ("<qt>%1&nbsp;MB</qt>").arg (sys.GetMinGuestRAM()));
    mLbRamMax->setText (tr ("<qt>%1&nbsp;MB</qt>").arg (sys.GetMaxGuestRAM()));
    mLbVideoMin->setText (tr ("<qt>%1&nbsp;MB</qt>").arg (sys.GetMinGuestVRAM()));
    mLbVideoMax->setText (tr ("<qt>%1&nbsp;MB</qt>").arg (sys.GetMaxGuestVRAM()));

    /* Retranslate the boot order items */
    QTreeWidgetItemIterator it (mTwBootOrder);
    while (*it) 
    {
        QTreeWidgetItem *item = (*it);
        item->setText (0, vboxGlobal().toString (
             static_cast<KDeviceType> (item->data (0, ITEM_TYPE_ROLE).toInt())));
        ++it;
    }
    /* Readjust the tree widget size */
    adjustBootOrderTWSize ();

    /* Shared Clipboard mode */
    mCbClipboard->setItemText (0, vboxGlobal().toString (KClipboardMode_Disabled));
    mCbClipboard->setItemText (1, vboxGlobal().toString (KClipboardMode_HostToGuest));
    mCbClipboard->setItemText (2, vboxGlobal().toString (KClipboardMode_GuestToHost));
    mCbClipboard->setItemText (3, vboxGlobal().toString (KClipboardMode_Bidirectional));

    /* IDE Controller Type */
    mCbIDEController->setItemText (0, vboxGlobal().toString (KIDEControllerType_PIIX3));
    mCbIDEController->setItemText (1, vboxGlobal().toString (KIDEControllerType_PIIX4));
}


void VBoxVMSettingsGeneral::valueChangedRAM (int aVal)
{
    mLeRam->setText (QString().setNum (aVal));
}

void VBoxVMSettingsGeneral::textChangedRAM (const QString &aText)
{
    mSlRam->setValue (aText.toInt());
}

void VBoxVMSettingsGeneral::valueChangedVRAM (int aVal)
{
    mLeVideo->setText (QString().setNum (aVal));
}

void VBoxVMSettingsGeneral::textChangedVRAM (const QString &aText)
{
    mSlVideo->setValue (aText.toInt());
}

void VBoxVMSettingsGeneral::moveBootItemUp()
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

void VBoxVMSettingsGeneral::moveBootItemDown()
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

void VBoxVMSettingsGeneral::onCurrentBootItemChanged (QTreeWidgetItem *aItem,
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

void VBoxVMSettingsGeneral::selectSnapshotFolder()
{
    QString settingsFolder = VBoxGlobal::getFirstExistingDir (mLeSnapshot->text());
    if (settingsFolder.isNull())
        settingsFolder = QFileInfo (mMachine.GetSettingsFilePath()).absolutePath();

    QString folder = vboxGlobal().getExistingDirectory (settingsFolder, this);
    if (folder.isNull())
        return;

    folder = QDir::convertSeparators (folder);
    /* Remove trailing slash if any */
    folder.remove (QRegExp ("[\\\\/]$"));

    /* Do this instead of le->setText (folder) to cause
     * isModified() return true */
    mLeSnapshot->selectAll();
    mLeSnapshot->insert (folder);
}

void VBoxVMSettingsGeneral::resetSnapshotFolder()
{
    /* Do this instead of le->setText (QString::null) to cause
     * isModified() return true */
    mLeSnapshot->selectAll();
    mLeSnapshot->del();
}

bool VBoxVMSettingsGeneral::eventFilter (QObject *aObject, QEvent *aEvent)
{
    if (!aObject->isWidgetType())
        return QWidget::eventFilter (aObject, aEvent);

    QWidget *widget = static_cast<QWidget*> (aObject);
    if (widget->topLevelWidget() != topLevelWidget())
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

void VBoxVMSettingsGeneral::adjustBootOrderTWSize ()
{
    if (mTwBootOrder)
    {
        /* Calculate the optimal size of the tree widget & set it as fixed
         * size. */
        mTwBootOrder->setFixedSize (static_cast<QAbstractItemView*> (mTwBootOrder)->sizeHintForColumn (0) + 2 * mTwBootOrder->frameWidth(),
                                    static_cast<QAbstractItemView*> (mTwBootOrder)->sizeHintForRow (0) * mTwBootOrder->topLevelItemCount() + 2 * mTwBootOrder->frameWidth());
    }
}
