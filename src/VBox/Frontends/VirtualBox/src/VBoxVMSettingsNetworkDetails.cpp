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
#else
    ,Qt::Tool
#endif /* Q_WS_MAC */
    )
    , mType (KNetworkAttachmentType_Null)
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsNetworkDetails::setupUi (this);

    /* Setup alternative widgets */
    mCbINT->setInsertPolicy (QComboBox::NoInsert);
    mLeIPv4->setValidator (new QRegExpValidator
        (QRegExp ("\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}"), this));
    mLeHMv4->setValidator (new QRegExpValidator
        (QRegExp ("\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}"), this));
    //mLeIPv6->setValidator (new QRegExpValidator
    //    (QRegExp ("\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}"), this));
    //mLeHMv6->setValidator (new QRegExpValidator
    //    (QRegExp ("\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}"), this));
    connect (mCbHOI, SIGNAL (currentIndexChanged (int)),
             this, SLOT (hostOnlyInterfaceChanged()));
#if defined (Q_WS_WIN32)
    mTbAdd->setIcon (VBoxGlobal::iconSet (":/add_host_iface_16px.png",
                                          ":/add_host_iface_disabled_16px.png"));
    mTbDel->setIcon (VBoxGlobal::iconSet (":/remove_host_iface_16px.png",
                                          ":/remove_host_iface_disabled_16px.png"));
    connect (mTbAdd, SIGNAL (clicked()), this, SLOT (addInterface()));
    connect (mTbDel, SIGNAL (clicked()), this, SLOT (delInterface()));
    connect (mRbAuto, SIGNAL (toggled (bool)),
             this, SLOT (hostOnlyDHCPChanged()));
    connect (mRbManual, SIGNAL (toggled (bool)),
             this, SLOT (hostOnlyDHCPChanged()));
#endif

    /* Setup common widgets */
    mLeMAC->setValidator (new QRegExpValidator
        (QRegExp ("[0-9A-Fa-f][02468ACEace][0-9A-Fa-f]{10}"), this));
    QStyleOption options;
    options.initFrom (mLeMAC);
    QSize contentSize (mLeMAC->fontMetrics().width ('X') * 12,
                       mLeMAC->fontMetrics().xHeight());
    QSize totalSize = style()->sizeFromContents (QStyle::CT_LineEdit,
                                                 &options, contentSize, this);
    mLeMAC->setMinimumWidth (totalSize.width());
    connect (mTbMAC, SIGNAL (clicked()), this, SLOT (genMACClicked()));
#if defined (Q_WS_MAC)
    /* Remove tool-button border at MAC */
    mTbMAC->setStyleSheet ("QToolButton {border: 0px none black;}");
#endif /* Q_WS_MAC */
}

void VBoxVMSettingsNetworkDetails::getFromAdapter (const CNetworkAdapter &aAdapter)
{
    mAdapter = aAdapter;

    /* Load alternate attributes */
    QString intName (mAdapter.GetInternalNetwork());
    if (!intName.isEmpty())
        setProperty ("INT_Name", QVariant (intName));

    QString ifsName (mAdapter.GetHostInterface());
    CHostNetworkInterface ifs =
        vboxGlobal().virtualBox().GetHost().FindHostNetworkInterfaceByName (ifsName);
    if (!ifs.isNull() && ifs.GetInterfaceType() == KHostNetworkInterfaceType_Bridged)
    {
        setProperty ("BRG_Name", QVariant (ifsName));
    }
    else if (!ifs.isNull() && ifs.GetInterfaceType() == KHostNetworkInterfaceType_HostOnly)
    {
        setProperty ("HOI_Name", QVariant (ifsName));
        setProperty ("HOI_DhcpEnabled", QVariant (ifs.GetDhcpEnabled()));
#if defined (Q_WS_WIN32)
        setProperty ("HOI_IPv6Supported", QVariant (ifs.GetIPV6Supported()));
#endif
        if (!ifs.GetDhcpEnabled())
        {
            setProperty ("HOI_IPv4Addr", QVariant (ifs.GetIPAddress()));
            setProperty ("HOI_IPv4Mask", QVariant (ifs.GetNetworkMask()));
            if (ifs.GetIPV6Supported())
            {
                setProperty ("HOI_IPv6Addr", QVariant (ifs.GetIPV6Address()));
                setProperty ("HOI_IPv6Mask", QVariant (QString::number (ifs.GetIPV6NetworkMaskPrefixLength())));
            }
        }
    }

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
        {
            mAdapter.SetHostInterface (name);
            break;
        }
        case KNetworkAttachmentType_Internal:
        {
            mAdapter.SetInternalNetwork (name);
            break;
        }
        case KNetworkAttachmentType_HostOnly:
        {
            CHostNetworkInterface iface =
                vboxGlobal().virtualBox().GetHost().FindHostNetworkInterfaceByName (name);
            if (!iface.isNull())
            {
                if (property ("HOI_DhcpEnabled").toBool())
                {
                    iface.EnableDynamicIpConfig();
                }
                else
                {
                    iface.EnableStaticIpConfig (property ("HOI_IPv4Addr").toString(),
                                                property ("HOI_IPv4Mask").toString());

                    if (property ("HOI_IPv6Supported").toBool())
                    {
                        iface.EnableStaticIpConfigV6 (property ("HOI_IPv6Addr").toString(),
                                                      property ("HOI_IPv6Mask").toString().toULong());
                    }
                }
            }
            mAdapter.SetHostInterface (name);
            break;
        }
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
    mLbIPv4->setVisible (mType == KNetworkAttachmentType_HostOnly);
    mLeIPv4->setVisible (mType == KNetworkAttachmentType_HostOnly);
    mLbHMv4->setVisible (mType == KNetworkAttachmentType_HostOnly);
    mLeHMv4->setVisible (mType == KNetworkAttachmentType_HostOnly);
    mLbIPv6->setVisible (mType == KNetworkAttachmentType_HostOnly);
    mLeIPv6->setVisible (mType == KNetworkAttachmentType_HostOnly);
    mLbHMv6->setVisible (mType == KNetworkAttachmentType_HostOnly);
    mLeHMv6->setVisible (mType == KNetworkAttachmentType_HostOnly);
#if defined (Q_WS_WIN32)
    mTbAdd->setVisible (mType == KNetworkAttachmentType_HostOnly);
    mTbDel->setVisible (mType == KNetworkAttachmentType_HostOnly);
    mLbCT->setVisible (mType == KNetworkAttachmentType_HostOnly);
    mRbAuto->setVisible (mType == KNetworkAttachmentType_HostOnly);
    mRbManual->setVisible (mType == KNetworkAttachmentType_HostOnly);
#endif

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

    /* Reload alternate combo-box dependences */
    if (mType == KNetworkAttachmentType_HostOnly)
    {
        hostOnlyInterfaceChanged();
        if (property ("HOI_DhcpEnabled").toBool())
        {
            mRbAuto->setChecked (true);
        }
        else
        {
            mRbManual->setChecked (true);
            QString ipv4 (property ("HOI_IPv4Addr").toString());
            if (!ipv4.isEmpty() && ipv4 != mLeIPv4->text())
                mLeIPv4->setText (ipv4);
            QString nmv4 (property ("HOI_IPv4Mask").toString());
            if (!nmv4.isEmpty() && nmv4 != mLeHMv4->text())
                mLeHMv4->setText (nmv4);
            if (property ("HOI_IPv6Supported").toBool())
            {
                QString ipv6 (property ("HOI_IPv6Addr").toString());
                if (!ipv6.isEmpty() && ipv6 != mLeIPv6->text())
                    mLeIPv6->setText (ipv6);
                QString nmv6 (property ("HOI_IPv6Mask").toString());
                if (!nmv6.isEmpty() && nmv6 != mLeHMv6->text())
                    mLeHMv6->setText (nmv6);
            }
        }
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
                aWarning = tr ("no host-only interface is selected");
                return false;
            }
            else if (!property ("HOI_DhcpEnabled").toBool())
            {
                if (property ("HOI_IPv4Addr").toString().isEmpty() ||
                    QHostAddress (property ("HOI_IPv4Addr").toString()).protocol()
                    != QAbstractSocket::IPv4Protocol)
                {
                    aWarning = tr ("host IPv4 address is wrong");
                    return false;
                }
                if (property ("HOI_IPv4Mask").toString().isEmpty() ||
                    QHostAddress (property ("HOI_IPv4Mask").toString()).protocol()
                    != QAbstractSocket::IPv4Protocol)
                {
                    aWarning = tr ("host IPv4 network mask is wrong");
                    return false;
                }
                if (property ("HOI_IPv6Supported").toBool())
                {
                    if (property ("HOI_IPv6Addr").toString().isEmpty() ||
                        QHostAddress (property ("HOI_IPv6Addr").toString()).protocol()
                        != QAbstractSocket::IPv6Protocol)
                    {
                        aWarning = tr ("host IPv6 address is wrong");
                        return false;
                    }
                }
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
    mNotSelected = tr ("Not Selected");
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
            setProperty ("HOI_DhcpEnabled", QVariant (mRbAuto->isChecked()));
            setProperty ("HOI_IPv6Supported", QVariant (mLeIPv6->isEnabled()));
            setProperty ("HOI_IPv4Addr", QVariant (mLeIPv4->text()));
            setProperty ("HOI_IPv4Mask", QVariant (mLeHMv4->text()));
            setProperty ("HOI_IPv6Addr", QVariant (mLeIPv6->text()));
            setProperty ("HOI_IPv6Mask", QVariant (mLeHMv6->text()));
            break;
        default:
            break;
    }

    setProperty ("MAC_Address", QVariant (mLeMAC->text()));
    setProperty ("Cable_Connected", QVariant (mCbCable->isChecked()));

    QIDialog::accept();
}

void VBoxVMSettingsNetworkDetails::hostOnlyInterfaceChanged()
{
#if defined (Q_WS_WIN32)
    CHostNetworkInterface iface =
        vboxGlobal().virtualBox().GetHost().FindHostNetworkInterfaceByName (mCbHOI->currentText());

    bool isValid = !iface.isNull() &&
                   iface.GetInterfaceType() == KHostNetworkInterfaceType_HostOnly;
    bool isManual = isValid && !iface.GetDhcpEnabled();

    mLbCT->setEnabled (isValid);
    mRbAuto->setEnabled (isValid);
    mRbManual->setEnabled (isValid);

    if (isManual)
    {
        mRbManual->blockSignals (true);
        mRbManual->setChecked (true);
        mRbManual->blockSignals (false);
    }
    else
    {
        mRbAuto->blockSignals (true);
        mRbAuto->setChecked (true);
        mRbAuto->blockSignals (false);
    }

    mTbDel->setEnabled (isValid);
#endif

    hostOnlyDHCPChanged();
}

void VBoxVMSettingsNetworkDetails::hostOnlyDHCPChanged()
{
    CHostNetworkInterface iface =
        vboxGlobal().virtualBox().GetHost().FindHostNetworkInterfaceByName (mCbHOI->currentText());

    bool isManual = !iface.isNull() &&
                    iface.GetInterfaceType() == KHostNetworkInterfaceType_HostOnly &&
                    mRbManual->isChecked();
    bool isIPv6Supported = isManual && iface.GetIPV6Supported();

    mLeIPv4->clear();
    mLeHMv4->clear();
    mLeIPv6->clear();
    mLeHMv6->clear();

    mLbIPv4->setEnabled (isManual);
    mLbHMv4->setEnabled (isManual);
    mLeIPv4->setEnabled (isManual);
    mLeHMv4->setEnabled (isManual);
    mLbIPv6->setEnabled (isIPv6Supported);
    mLbHMv6->setEnabled (isIPv6Supported);
    mLeIPv6->setEnabled (isIPv6Supported);
    mLeHMv6->setEnabled (isIPv6Supported);

    if (isManual)
    {
        mLeIPv4->setText (iface.GetIPAddress());
        mLeHMv4->setText (iface.GetNetworkMask());

        if (isIPv6Supported)
        {
            mLeIPv6->setText (iface.GetIPV6Address());
            mLeHMv6->setText (QString::number (iface.GetIPV6NetworkMaskPrefixLength()));
        }
    }
}

void VBoxVMSettingsNetworkDetails::genMACClicked()
{
    mAdapter.SetMACAddress (QString::null);
    mLeMAC->setText (mAdapter.GetMACAddress());
}

#if defined (Q_WS_WIN32)
void VBoxVMSettingsNetworkDetails::addInterface()
{
    /* Allow the started helper process to make itself the foreground window */
    AllowSetForegroundWindow (ASFW_ANY);

    /* Creating interface */
    CHost host = vboxGlobal().virtualBox().GetHost();
    CHostNetworkInterface iface;
    CProgress progress = host.CreateHostOnlyNetworkInterface (iface);
    if (host.isOk())
    {
        vboxProblem().showModalProgressDialog (progress,
            tr ("Performing", "creating/removing host-only interface"), this);
        if (progress.GetResultCode() == 0)
        {
            mCbHOI->insertItem (mCbHOI->count(), iface.GetName());
            mCbHOI->setCurrentIndex (mCbHOI->count() - 1);
        }
        else
            vboxProblem().cannotCreateHostInterface (progress, this);
    }
    else
        vboxProblem().cannotCreateHostInterface (host, this);

    /* Allow the started helper process to make itself the foreground window */
    AllowSetForegroundWindow (ASFW_ANY);
}

void VBoxVMSettingsNetworkDetails::delInterface()
{
    /* Allow the started helper process to make itself the foreground window */
    AllowSetForegroundWindow (ASFW_ANY);

    Assert (mCbHOI->itemData (mCbHOI->currentIndex()).toString()
            != QString (emptyItemCode));

    /* Check interface name */
    QString name (mCbHOI->currentText());

    /* Asking user about deleting selected network interface */
    int delNetIface = vboxProblem().confirmDeletingHostInterface (name, this);
    if (delNetIface == QIMessageBox::Cancel) return;

    /* Removing interface */
    CHost host = vboxGlobal().virtualBox().GetHost();
    CHostNetworkInterface iface = host.FindHostNetworkInterfaceByName (name);
    if (!iface.isNull())
    {
        /* Delete interface */
        CProgress progress = host.RemoveHostOnlyNetworkInterface (iface.GetId(), iface);
        if (host.isOk())
        {
            vboxProblem().showModalProgressDialog (progress,
                tr ("Performing", "creating/removing host-only interface"), this);
            if (progress.GetResultCode() == 0)
            {
                int pos = mCbHOI->findText (name);
                Assert (pos != -1);
                mCbHOI->removeItem (pos);
            }
            else
                vboxProblem().cannotRemoveHostInterface (progress, iface, this);
        }
    }
    if (!host.isOk())
        vboxProblem().cannotRemoveHostInterface (host, iface, this);

    /* Allow the started helper process to make itself the foreground window */
    AllowSetForegroundWindow (ASFW_ANY);
}
#endif

void VBoxVMSettingsNetworkDetails::populateComboboxes()
{
    /* Append || retranslate <Not Selected> item */
    QList <QComboBox*> list;
    list << mCbBRG << mCbINT << mCbHOI;

    foreach (QComboBox *cb, list)
    {
        int pos = cb->findData (emptyItemCode);
        if (pos == -1)
            cb->insertItem (0, mNotSelected, emptyItemCode);
        else
            cb->setItemText (pos, mNotSelected);
    }
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

