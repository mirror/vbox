/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsNetworkDetails class implementation
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

/* VBox Includes */
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "VBoxVMSettingsNetwork.h"
#include "VBoxVMSettingsNetworkDetails.h"

/* Qt Includes */
#include <QHostAddress>

/* Empty item extra-code */
const char *emptyItemCode = "#empty#";

/* VBoxVMSettingsNetwork Stuff */
VBoxVMSettingsNetworkDetails::VBoxVMSettingsNetworkDetails (QWidget *aParent)
    : QIWithRetranslateUI2 <QIDialog> (aParent
#ifdef Q_WS_MAC
    ,Qt::Sheet
#endif /* Q_WS_MAC */
    )
    , mType (KNetworkAttachmentType_Null)
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsNetworkDetails::setupUi (this);

    /* Setup alternative widgets */
    mCbINT->setInsertPolicy (QComboBox::NoInsert);

    /* Setup common widgets */
    mLeMAC->setValidator (new QRegExpValidator
        (QRegExp ("[0-9A-Fa-f][02468ACEace][0-9A-Fa-f]{10}"), this));
    QStyleOptionFrame sof;
    sof.initFrom (mLeMAC);
    sof.rect = mLeMAC->contentsRect();
    sof.lineWidth = mLeMAC->style()->pixelMetric (QStyle::PM_DefaultFrameWidth);
    sof.midLineWidth = 0;
    sof.state |= QStyle::State_Sunken;
    QSize sc (mLeMAC->fontMetrics().width (QString().fill ('X', 12)) + 2*2,
              mLeMAC->fontMetrics().xHeight()                        + 2*1);
    QSize sa = mLeMAC->style()->sizeFromContents (QStyle::CT_LineEdit, &sof, sc, mLeMAC);
    mLeMAC->setMinimumWidth (sa.width());
    connect (mTbMAC, SIGNAL (clicked()), this, SLOT (genMACClicked()));
#if defined (Q_WS_MAC)
    /* Remove tool-button border at MAC */
    mTbMAC->setStyleSheet ("QToolButton {border: 0px none black;}");
#endif /* Q_WS_MAC */
}

void VBoxVMSettingsNetworkDetails::getFromAdapter (const CNetworkAdapter &aAdapter)
{
    mAdapter = aAdapter;

    /* Load alternate settings */
    QString intName (mAdapter.GetInternalNetwork());
    if (!intName.isEmpty())
        setProperty ("INT_Name", QVariant (intName));
    QString bridgedIfName (mAdapter.GetBridgedInterface());
    CHostNetworkInterface bridgedIf =
        vboxGlobal().virtualBox().GetHost().FindHostNetworkInterfaceByName (bridgedIfName);
    if (!bridgedIf.isNull() && bridgedIf.GetInterfaceType() == KHostNetworkInterfaceType_Bridged)
        setProperty ("BRG_Name", QVariant (bridgedIfName));

    QString hostonlyIfName (mAdapter.GetHostOnlyInterface());
    CHostNetworkInterface hostonlyIf =
        vboxGlobal().virtualBox().GetHost().FindHostNetworkInterfaceByName (hostonlyIfName);
    if (!hostonlyIf.isNull() && hostonlyIf.GetInterfaceType() == KHostNetworkInterfaceType_HostOnly)
        setProperty ("HOI_Name", QVariant (hostonlyIfName));

    /* Load common settings */
    setProperty ("MAC_Address", QVariant (aAdapter.GetMACAddress()));
    setProperty ("Cable_Connected", QVariant (aAdapter.GetCableConnected()));
}

void VBoxVMSettingsNetworkDetails::putBackToAdapter()
{
    /* Save alternative settings */
    QString name (currentName());
    switch (mType)
    {
        case KNetworkAttachmentType_Bridged:
            mAdapter.SetBridgedInterface (name);
            break;
        case KNetworkAttachmentType_Internal:
            mAdapter.SetInternalNetwork (name);
            break;
        case KNetworkAttachmentType_HostOnly:
            mAdapter.SetHostOnlyInterface (name);
            break;
        default:
            break;
    }

    /* Save common settings */
    mAdapter.SetMACAddress (property ("MAC_Address").toString());
    mAdapter.SetCableConnected (property ("Cable_Connected").toBool());
}

void VBoxVMSettingsNetworkDetails::loadList (KNetworkAttachmentType aType,
                                             const QStringList &aList)
{
    mType = aType;

    /* Setup visibility for alternate widgets */
    mLsHost->setVisible (mType != KNetworkAttachmentType_Null &&
                         mType != KNetworkAttachmentType_NAT);
    mLbBRG->setVisible (mType == KNetworkAttachmentType_Bridged);
    mCbBRG->setVisible (mType == KNetworkAttachmentType_Bridged);
    mLbINT->setVisible (mType == KNetworkAttachmentType_Internal);
    mCbINT->setVisible (mType == KNetworkAttachmentType_Internal);
    mLbHOI->setVisible (mType == KNetworkAttachmentType_HostOnly);
    mCbHOI->setVisible (mType == KNetworkAttachmentType_HostOnly);

    /* Repopulate alternate combo-box with items */
    if (mType != KNetworkAttachmentType_Null &&
        mType != KNetworkAttachmentType_NAT)
    {
        comboBox()->clear();
        populateComboboxes();
        comboBox()->insertItems (comboBox()->count(), aList);
        int pos = comboBox()->findText (currentName());
        comboBox()->setCurrentIndex (pos == -1 ? 0 : pos);
    }

    /* Load common settings */
    mLeMAC->setText (property ("MAC_Address").toString());
    mCbCable->setChecked (property ("Cable_Connected").toBool());

    /* Applying language settings */
    retranslateUi();
}

bool VBoxVMSettingsNetworkDetails::revalidate (KNetworkAttachmentType aType, QString &aWarning)
{
    switch (aType)
    {
        case KNetworkAttachmentType_Bridged:
            if (currentName (aType).isNull())
            {
                aWarning = tr ("no bridged network adapter is selected");
                return false;
            }
            break;
        case KNetworkAttachmentType_Internal:
            if (currentName (aType).isNull())
            {
                aWarning = tr ("no internal network name is specified");
                return false;
            }
            break;
        case KNetworkAttachmentType_HostOnly:
            if (currentName (aType).isNull())
            {
                aWarning = tr ("no host-only adapter is selected");
                return false;
            }
            break;
        default:
            break;
    }
    return true;
}

QString VBoxVMSettingsNetworkDetails::currentName (KNetworkAttachmentType aType) const
{
    if (aType == KNetworkAttachmentType_Null)
        aType = mType;

    QString result;
    switch (aType)
    {
        case KNetworkAttachmentType_Bridged:
            result = property ("BRG_Name").toString();
            break;
        case KNetworkAttachmentType_Internal:
            result = property ("INT_Name").toString();
            break;
        case KNetworkAttachmentType_HostOnly:
            result = property ("HOI_Name").toString();
            break;
        default:
            break;
    }
    return result.isEmpty() ? QString::null : result;
}

void VBoxVMSettingsNetworkDetails::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsNetworkDetails::retranslateUi (this);

    /* Translate window title */
    switch (mType)
    {
        case KNetworkAttachmentType_Null:
        case KNetworkAttachmentType_NAT:
            setWindowTitle (tr ("Basic Details"));
            break;
        case KNetworkAttachmentType_Bridged:
            setWindowTitle (tr ("Bridged Network Details"));
            break;
        case KNetworkAttachmentType_Internal:
            setWindowTitle (tr ("Internal Network Details"));
            break;
        case KNetworkAttachmentType_HostOnly:
            setWindowTitle (tr ("Host-only Network Details"));
            break;
    }

    /* Translate empty items */
    populateComboboxes();
}

void VBoxVMSettingsNetworkDetails::showEvent (QShowEvent *aEvent)
{
    /* Update full layout system of message window */
    QList <QLayout*> layouts = findChildren <QLayout*> ();
    foreach (QLayout *item, layouts)
    {
        item->update();
        item->activate();
    }
    qApp->processEvents();

    /* Now resize window to minimum possible size */
    resize (minimumSizeHint());
    qApp->processEvents();
    setFixedSize (minimumSizeHint());

    /* Centering widget */
    VBoxGlobal::centerWidget (this, parentWidget(), false);

    QIDialog::showEvent (aEvent);
}

void VBoxVMSettingsNetworkDetails::accept()
{
    /* Save temporary attributes as dynamic properties */
    switch (mType)
    {
        case KNetworkAttachmentType_Bridged:
            setProperty ("BRG_Name",
                         QVariant (mCbBRG->itemData (mCbBRG->currentIndex()).toString() == QString (emptyItemCode) ?
                                   QString::null : mCbBRG->currentText()));
            break;
        case KNetworkAttachmentType_Internal:
            setProperty ("INT_Name",
                         QVariant (mCbINT->itemData (mCbINT->currentIndex()).toString() == QString (emptyItemCode) &&
                                   mCbINT->currentText() == mCbINT->itemText (mCbINT->currentIndex()) ?
                                   QString::null : mCbINT->currentText()));
            break;
        case KNetworkAttachmentType_HostOnly:
            setProperty ("HOI_Name",
                         QVariant (mCbHOI->itemData (mCbHOI->currentIndex()).toString() == QString (emptyItemCode) ?
                                   QString::null : mCbHOI->currentText()));
            break;
        default:
            break;
    }
    setProperty ("MAC_Address", QVariant (mLeMAC->text()));
    setProperty ("Cable_Connected", QVariant (mCbCable->isChecked()));

    QIDialog::accept();
}

void VBoxVMSettingsNetworkDetails::genMACClicked()
{
    mAdapter.SetMACAddress (QString::null);
    mLeMAC->setText (mAdapter.GetMACAddress());
}

void VBoxVMSettingsNetworkDetails::populateComboboxes()
{
    {   /* Bridged adapters combo-box */
        int pos = mCbBRG->findData (emptyItemCode);
        if (pos == -1)
            mCbBRG->insertItem (0,
                VBoxVMSettingsNetwork::tr ("Not selected", "adapter"),
                emptyItemCode);
        else
            mCbBRG->setItemText (pos,
                VBoxVMSettingsNetwork::tr ("Not selected", "adapter"));
    }   /* Bridged adapters combo-box */

    {   /* Internal networks combo-box */
        int pos = mCbINT->findData (emptyItemCode);
        if (pos == -1)
            mCbINT->insertItem (0,
                VBoxVMSettingsNetwork::tr ("Not selected", "network"),
                emptyItemCode);
        else
            mCbINT->setItemText (pos,
                VBoxVMSettingsNetwork::tr ("Not selected", "network"));
    }   /* Internal networks combo-box */

    {   /* Host-only adapters combo-box */
        int pos = mCbHOI->findData (emptyItemCode);
        if (pos == -1)
            mCbHOI->insertItem (0,
                VBoxVMSettingsNetwork::tr ("Not selected", "adapter"),
                emptyItemCode);
        else
            mCbHOI->setItemText (pos,
                VBoxVMSettingsNetwork::tr ("Not selected", "adapter"));
    }   /* Host-only adapters combo-box */
}

QComboBox* VBoxVMSettingsNetworkDetails::comboBox() const
{
    switch (mType)
    {
        case KNetworkAttachmentType_Bridged:
            return mCbBRG;
        case KNetworkAttachmentType_Internal:
            return mCbINT;
        case KNetworkAttachmentType_HostOnly:
            return mCbHOI;
        default:
            break;
    }
    return 0;
}

