/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIIndicatorsPool class implementation
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QTimer>

/* GUI includes: */
#include "UIIndicatorsPool.h"
#include "VBoxGlobal.h"
#include "UIMachineDefs.h"
#include "QIWithRetranslateUI.h"

/* COM includes: */
#include "CSystemProperties.h"
#include "CMachineDebugger.h"
#include "CGuest.h"
#include "CStorageController.h"
#include "CMediumAttachment.h"
#include "CNetworkAdapter.h"
#include "CUSBController.h"
#include "CUSBDevice.h"
#include "CSharedFolder.h"
#include "CVRDEServer.h"

/* Other VBox includes: */
#include <iprt/time.h>

class UIIndicatorHardDisks : public QIWithRetranslateUI<QIStateIndicator>
{
    Q_OBJECT;

public:

    UIIndicatorHardDisks(CSession &session)
      : QIWithRetranslateUI<QIStateIndicator>()
      , m_session(session)
    {
        setStateIcon(KDeviceActivity_Idle, QPixmap(":/hd_16px.png"));
        setStateIcon(KDeviceActivity_Reading, QPixmap(":/hd_read_16px.png"));
        setStateIcon(KDeviceActivity_Writing, QPixmap(":/hd_write_16px.png"));
        setStateIcon(KDeviceActivity_Null, QPixmap(":/hd_disabled_16px.png"));

        retranslateUi();
    }

    void retranslateUi()
    {
        updateAppearance();
    }

    void updateAppearance()
    {
        const CMachine &machine = m_session.GetMachine();

        QString strToolTip = QApplication::translate("UIIndicatorsPool", "<p style='white-space:pre'><nobr>Indicates the activity "
                                                     "of the virtual hard disks:</nobr>%1</p>", "HDD tooltip");

        QString strFullData;
        bool fAttachmentsPresent = false;

        const CStorageControllerVector &controllers = machine.GetStorageControllers();
        foreach (const CStorageController &controller, controllers)
        {
            QString strAttData;
            const CMediumAttachmentVector &attachments = machine.GetMediumAttachmentsOfController(controller.GetName());
            foreach (const CMediumAttachment &attachment, attachments)
            {
                if (attachment.GetType() != KDeviceType_HardDisk)
                    continue;
                strAttData += QString("<br>&nbsp;<nobr>%1:&nbsp;%2</nobr>")
                    .arg(vboxGlobal().toString(StorageSlot(controller.GetBus(), attachment.GetPort(), attachment.GetDevice())))
                    .arg(VBoxMedium(attachment.GetMedium(), VBoxDefs::MediumType_HardDisk).location());
                fAttachmentsPresent = true;
            }
            if (!strAttData.isNull())
                strFullData += QString("<br><nobr><b>%1</b></nobr>").arg(controller.GetName()) + strAttData;
        }

        /* For now we will hide that LED at all if there are no attachments! */
        if (!fAttachmentsPresent)
            setHidden(true);
        //if (!fAttachmentsPresent)
        //    strFullData += QApplication::translate("UIIndicatorsPool", "<br><nobr><b>No hard disks attached</b></nobr>", "HDD tooltip");

        setToolTip(strToolTip.arg(strFullData));
        setState(fAttachmentsPresent ? KDeviceActivity_Idle : KDeviceActivity_Null);
    }

protected:
    /* For compatibility reason we do it here, later this should be moved to
     * QIStateIndicator. */
    CSession &m_session;
};

class UIIndicatorOpticalDisks : public QIWithRetranslateUI<QIStateIndicator>
{
    Q_OBJECT;

public:

    UIIndicatorOpticalDisks(CSession &session)
      : QIWithRetranslateUI<QIStateIndicator>()
      , m_session(session)
    {
        setStateIcon(KDeviceActivity_Idle, QPixmap(":/cd_16px.png"));
        setStateIcon(KDeviceActivity_Reading, QPixmap(":/cd_read_16px.png"));
        setStateIcon(KDeviceActivity_Writing, QPixmap(":/cd_write_16px.png"));
        setStateIcon(KDeviceActivity_Null, QPixmap(":/cd_disabled_16px.png"));

        retranslateUi();
    }

    void retranslateUi()
    {
        updateAppearance();
    }

    void updateAppearance()
    {
        const CMachine &machine = m_session.GetMachine();

        QString strToolTip = QApplication::translate("UIIndicatorsPool", "<p style='white-space:pre'><nobr>Indicates the activity "
                                                     "of the CD/DVD devices:</nobr>%1</p>", "CD/DVD tooltip");

        QString strFullData;
        bool fAttachmentsPresent = false;
        bool fAttachmentsMounted = false;

        const CStorageControllerVector &controllers = machine.GetStorageControllers();
        foreach (const CStorageController &controller, controllers)
        {
            QString strAttData;
            const CMediumAttachmentVector &attachments = machine.GetMediumAttachmentsOfController(controller.GetName());
            foreach (const CMediumAttachment &attachment, attachments)
            {
                if (attachment.GetType() != KDeviceType_DVD)
                    continue;
                VBoxMedium vboxMedium(attachment.GetMedium(), VBoxDefs::MediumType_DVD);
                strAttData += QString("<br>&nbsp;<nobr>%1:&nbsp;%2</nobr>")
                    .arg(vboxGlobal().toString(StorageSlot(controller.GetBus(), attachment.GetPort(), attachment.GetDevice())))
                    .arg(vboxMedium.isNull() || vboxMedium.isHostDrive() ? vboxMedium.name() : vboxMedium.location());
                fAttachmentsPresent = true;
                if (!vboxMedium.isNull())
                    fAttachmentsMounted = true;
            }
            if (!strAttData.isNull())
                strFullData += QString("<br><nobr><b>%1</b></nobr>").arg(controller.GetName()) + strAttData;
        }

        /* For now we will hide that LED at all if there are no attachments! */
        if (!fAttachmentsPresent)
            setHidden(true);
        //if (!fAttachmentsPresent)
        //    strFullData = QApplication::translate("UIIndicatorsPool", "<br><nobr><b>No CD/DVD devices attached</b></nobr>", "CD/DVD tooltip");

        setToolTip(strToolTip.arg(strFullData));
        setState(fAttachmentsMounted ? KDeviceActivity_Idle : KDeviceActivity_Null);
    }

protected:
    /* For compatibility reason we do it here, later this should be moved to
     * QIStateIndicator. */
    CSession &m_session;
};

class UIIndicatorFloppyDisks : public QIWithRetranslateUI<QIStateIndicator>
{
    Q_OBJECT;

public:

    UIIndicatorFloppyDisks(CSession &session)
      : QIWithRetranslateUI<QIStateIndicator>()
      , m_session(session)
    {
        setStateIcon(KDeviceActivity_Idle, QPixmap(":/fd_16px.png"));
        setStateIcon(KDeviceActivity_Reading, QPixmap(":/fd_read_16px.png"));
        setStateIcon(KDeviceActivity_Writing, QPixmap(":/fd_write_16px.png"));
        setStateIcon(KDeviceActivity_Null, QPixmap(":/fd_disabled_16px.png"));

        retranslateUi();
    }

    void retranslateUi()
    {
        updateAppearance();
    }

    void updateAppearance()
    {
        const CMachine &machine = m_session.GetMachine();

        QString strToolTip = QApplication::translate("UIIndicatorsPool", "<p style='white-space:pre'><nobr>Indicates the activity "
                                                     "of the floppy devices:</nobr>%1</p>", "FD tooltip");

        QString strFullData;
        bool fAttachmentsPresent = false;
        bool fAttachmentsMounted = false;

        const CStorageControllerVector &controllers = machine.GetStorageControllers();
        foreach (const CStorageController &controller, controllers)
        {
            QString strAttData;
            const CMediumAttachmentVector &attachments = machine.GetMediumAttachmentsOfController(controller.GetName());
            foreach (const CMediumAttachment &attachment, attachments)
            {
                if (attachment.GetType() != KDeviceType_Floppy)
                    continue;
                VBoxMedium vboxMedium(attachment.GetMedium(), VBoxDefs::MediumType_Floppy);
                strAttData += QString("<br>&nbsp;<nobr>%1:&nbsp;%2</nobr>")
                    .arg(vboxGlobal().toString(StorageSlot(controller.GetBus(), attachment.GetPort(), attachment.GetDevice())))
                    .arg(vboxMedium.isNull() || vboxMedium.isHostDrive() ? vboxMedium.name() : vboxMedium.location());
                fAttachmentsPresent = true;
                if (!vboxMedium.isNull())
                    fAttachmentsMounted = true;
            }
            if (!strAttData.isNull())
                strFullData += QString("<br><nobr><b>%1</b></nobr>").arg(controller.GetName()) + strAttData;
        }

        /* For now we will hide that LED at all if there are no attachments! */
        if (!fAttachmentsPresent)
            setHidden(true);
        //if (!fAttachmentsPresent)
        //    strFullData = QApplication::translate("UIIndicatorsPool", "<br><nobr><b>No floppy devices attached</b></nobr>", "FD tooltip");

        setToolTip(strToolTip.arg(strFullData));
        setState(fAttachmentsMounted ? KDeviceActivity_Idle : KDeviceActivity_Null);
    }

protected:
    /* For compatibility reason we do it here, later this should be moved to
     * QIStateIndicator. */
    CSession &m_session;
};

class UIIndicatorNetworkAdapters : public QIWithRetranslateUI<QIStateIndicator>
{
    Q_OBJECT;

public:

    UIIndicatorNetworkAdapters(CSession &session)
      : QIWithRetranslateUI<QIStateIndicator>()
      , m_session(session)
      , m_pUpdateTimer(new QTimer(this))
    {
        setStateIcon(KDeviceActivity_Idle, QPixmap(":/nw_16px.png"));
        setStateIcon(KDeviceActivity_Reading, QPixmap(":/nw_read_16px.png"));
        setStateIcon(KDeviceActivity_Writing, QPixmap(":/nw_write_16px.png"));
        setStateIcon(KDeviceActivity_Null, QPixmap(":/nw_disabled_16px.png"));

        connect(m_pUpdateTimer, SIGNAL(timeout()), SLOT(sltUpdateNetworkIPs()));
        m_pUpdateTimer->start(5000);

        retranslateUi();
    }

    void retranslateUi()
    {
        updateAppearance();
    }

    void updateAppearance()
    {
        const CMachine &machine = m_session.GetMachine();
        QString strFullData;

        ulong uMaxCount = vboxGlobal().virtualBox().GetSystemProperties().GetMaxNetworkAdapters(KChipsetType_PIIX3);

        QString strToolTip = QApplication::translate("UIIndicatorsPool",
                                 "<p style='white-space:pre'><nobr>Indicates the activity of the "
                                 "network interfaces:</nobr>%1</p>", "Network adapters tooltip");

        RTTIMESPEC time;
        uint64_t u64Now = RTTimeSpecGetNano(RTTimeNow(&time));

        QString strFlags, strCount;
        LONG64 iTimestamp;
        machine.GetGuestProperty("/VirtualBox/GuestInfo/Net/Count", strCount, iTimestamp, strFlags);
        bool fPropsValid = (u64Now - iTimestamp < UINT64_C(60000000000)); /* timeout beacon */

        QStringList ipList, macList;
        if (fPropsValid)
        {
            int cAdapters = RT_MIN(strCount.toInt(), (int)uMaxCount);
            for (int i = 0; i < cAdapters; ++i)
            {
                ipList << machine.GetGuestPropertyValue(QString("/VirtualBox/GuestInfo/Net/%1/V4/IP").arg(i));
                macList << machine.GetGuestPropertyValue(QString("/VirtualBox/GuestInfo/Net/%1/MAC").arg(i));
            }
        }

        ulong uEnabled = 0;
        for (ulong uSlot = 0; uSlot < uMaxCount; ++uSlot)
        {
            const CNetworkAdapter &adapter = machine.GetNetworkAdapter(uSlot);
            if (adapter.GetEnabled())
            {
                QString strGuestIp;
                if (fPropsValid)
                {
                    QString strGuestMac = adapter.GetMACAddress();
                    int iIp = macList.indexOf(strGuestMac);
                    if (iIp >= 0)
                        strGuestIp = ipList[iIp];
                }
                strFullData += QApplication::translate("UIIndicatorsPool",
                               "<br><nobr><b>Adapter %1 (%2)</b>: %3 cable %4</nobr>", "Network adapters tooltip")
                    .arg(uSlot + 1)
                    .arg(vboxGlobal().toString(adapter.GetAttachmentType()))
                    .arg(strGuestIp.isEmpty() ? "" : "IP " + strGuestIp + ", ")
                    .arg(adapter.GetCableConnected() ?
                         QApplication::translate("UIIndicatorsPool", "connected", "Network adapters tooltip") :
                         QApplication::translate("UIIndicatorsPool", "disconnected", "Network adapters tooltip"));
                ++uEnabled;
            }
        }

        setState(uEnabled > 0 ? KDeviceActivity_Idle : KDeviceActivity_Null);
        if (!uEnabled)
            setHidden(true);

        if (strFullData.isNull())
            strFullData = QApplication::translate("UIIndicatorsPool",
                              "<br><nobr><b>All network adapters are disabled</b></nobr>", "Network adapters tooltip");

        setToolTip(strToolTip.arg(strFullData));
    }
protected slots:

    void sltUpdateNetworkIPs()
    {
        updateAppearance();
    }

protected:
    /* For compatibility reason we do it here, later this should be moved to
     * QIStateIndicator. */
    CSession &m_session;
    QTimer *m_pUpdateTimer;
};

class UIIndicatorUSBDevices : public QIWithRetranslateUI<QIStateIndicator>
{
    Q_OBJECT;

public:

    UIIndicatorUSBDevices(CSession &session)
      : QIWithRetranslateUI<QIStateIndicator>()
      , m_session(session)
    {
        setStateIcon(KDeviceActivity_Idle, QPixmap(":/usb_16px.png"));
        setStateIcon(KDeviceActivity_Reading, QPixmap(":/usb_read_16px.png"));
        setStateIcon(KDeviceActivity_Writing, QPixmap(":/usb_write_16px.png"));
        setStateIcon(KDeviceActivity_Null, QPixmap(":/usb_disabled_16px.png"));

        retranslateUi();
    }

    void retranslateUi()
    {
        updateAppearance();
    }

    void updateAppearance()
    {
        const CMachine &machine = m_session.GetMachine();

        QString strToolTip = QApplication::translate("UIIndicatorsPool", "<p style='white-space:pre'><nobr>Indicates the activity of "
                                "the attached USB devices:</nobr>%1</p>", "USB device tooltip");
        QString strFullData;

        const CUSBController &usbctl = machine.GetUSBController();
        setState(!usbctl.isNull() && usbctl.GetEnabled() && usbctl.GetProxyAvailable() ? KDeviceActivity_Idle : KDeviceActivity_Null);
        if (!usbctl.isNull() && usbctl.GetEnabled())
        {
            const CConsole &console = m_session.GetConsole();

            const CUSBDeviceVector &devsvec = console.GetUSBDevices();
            for (int i = 0; i < devsvec.size(); ++ i)
            {
                CUSBDevice usb = devsvec[i];
                strFullData += QString("<br><b><nobr>%1</nobr></b>").arg(vboxGlobal().details(usb));
            }
            if (strFullData.isNull())
                strFullData = QApplication::translate("UIIndicatorsPool", "<br><nobr><b>No USB devices attached</b></nobr>", "USB device tooltip");
        }
        else
            strFullData = QApplication::translate("UIIndicatorsPool", "<br><nobr><b>USB Controller is disabled</b></nobr>", "USB device tooltip");

        setToolTip(strToolTip.arg(strFullData));
    }

protected:
    /* For compatibility reason we do it here, later this should be moved to
     * QIStateIndicator. */
    CSession &m_session;
};

class UIIndicatorSharedFolders : public QIWithRetranslateUI<QIStateIndicator>
{
    Q_OBJECT;

public:

    UIIndicatorSharedFolders(CSession &session)
      : QIWithRetranslateUI<QIStateIndicator>()
      , m_session(session)
    {
        setStateIcon(KDeviceActivity_Idle, QPixmap(":/shared_folder_16px.png"));
        setStateIcon(KDeviceActivity_Reading, QPixmap(":/shared_folder_read_16px.png"));
        setStateIcon(KDeviceActivity_Writing, QPixmap(":/shared_folder_write_16px.png"));
        setStateIcon(KDeviceActivity_Null, QPixmap(":/shared_folder_disabled_16px.png"));

        retranslateUi();
    }

    void retranslateUi()
    {
        updateAppearance();
    }

    void updateAppearance()
    {
        const CMachine &machine = m_session.GetMachine();
        const CConsole &console = m_session.GetConsole();

        QString strToolTip = QApplication::translate("UIIndicatorsPool", "<p style='white-space:pre'><nobr>Indicates the activity of "
                                "the machine's shared folders:</nobr>%1</p>", "Shared folders tooltip");

        QString strFullData;
        QMap<QString, QString> sfs;

        /* Permanent folders */
        const CSharedFolderVector &psfvec = machine.GetSharedFolders();

        for (int i = 0; i < psfvec.size(); ++ i)
        {
            const CSharedFolder &sf = psfvec[i];
            sfs.insert(sf.GetName(), sf.GetHostPath());
        }

        /* Transient folders */
        const CSharedFolderVector &tsfvec = console.GetSharedFolders();

        for (int i = 0; i < tsfvec.size(); ++ i)
        {
            const CSharedFolder &sf = tsfvec[i];
            sfs.insert(sf.GetName(), sf.GetHostPath());
        }

        for (QMap<QString, QString>::const_iterator it = sfs.constBegin(); it != sfs.constEnd(); ++ it)
        {
            /* Select slashes depending on the OS type */
            if (VBoxGlobal::isDOSType(console.GetGuest().GetOSTypeId()))
                strFullData += QString("<br><nobr><b>\\\\vboxsvr\\%1&nbsp;</b></nobr><nobr>%2</nobr>")
                                       .arg(it.key(), it.value());
            else
                strFullData += QString("<br><nobr><b>%1&nbsp;</b></nobr><nobr>%2</nobr>")
                                       .arg(it.key(), it.value());
        }

        if (sfs.count() == 0)
            strFullData = QApplication::translate("UIIndicatorsPool", "<br><nobr><b>No shared folders</b></nobr>", "Shared folders tooltip");

        setState(!sfs.isEmpty() ? KDeviceActivity_Idle : KDeviceActivity_Null);
        setToolTip(strToolTip.arg(strFullData));
    }

protected:
    /* For compatibility reason we do it here, later this should be moved to
     * QIStateIndicator. */
    CSession &m_session;
};

class UIIndicatorVRDEDisks : public QIWithRetranslateUI<QIStateIndicator>
{
    Q_OBJECT;

public:

    UIIndicatorVRDEDisks(CSession &session)
      : QIWithRetranslateUI<QIStateIndicator>()
      , m_session(session)
    {
        setStateIcon(0, QPixmap (":/vrdp_disabled_16px.png"));
        setStateIcon(1, QPixmap (":/vrdp_16px.png"));

        retranslateUi();
    }

    void retranslateUi()
    {
        updateAppearance();
    }

    void updateAppearance()
    {
        CVRDEServer srv = m_session.GetMachine().GetVRDEServer();
        if (!srv.isNull())
        {
            /* update menu&status icon state */
            bool fEnabled = srv.GetEnabled();

            setState(fEnabled ? KDeviceActivity_Idle : KDeviceActivity_Null);

            QString tip = QApplication::translate("UIIndicatorsPool", "Indicates whether the Remote Desktop Server "
                             "is enabled (<img src=:/vrdp_16px.png/>) or not "
                             "(<img src=:/vrdp_disabled_16px.png/>).");
            if (srv.GetEnabled())
                tip += QApplication::translate("UIIndicatorsPool", "<hr>The Remote Desktop Server is listening on port %1").arg(srv.GetVRDEProperty("TCP/Ports"));
            setToolTip(tip);
        }
    }

protected:
    /* For compatibility reason we do it here, later this should be moved to
     * QIStateIndicator. */
    CSession &m_session;
};

class UIIndicatorVirtualization : public QIWithRetranslateUI<QIStateIndicator>
{
    Q_OBJECT;

public:

    UIIndicatorVirtualization(CSession &session)
      : QIWithRetranslateUI<QIStateIndicator>()
      , m_session(session)
    {
        setStateIcon(0, QPixmap(":/vtx_amdv_disabled_16px.png"));
        setStateIcon(1, QPixmap(":/vtx_amdv_16px.png"));

        retranslateUi();
    }

    void retranslateUi()
    {
        updateAppearance();
    }

    void updateAppearance()
    {
        const CConsole &console = m_session.GetConsole();
        if (console.isNull())
            return;

        const CMachineDebugger &debugger = console.GetDebugger();
        if (debugger.isNull())
            return;

        bool bVirtEnabled = debugger.GetHWVirtExEnabled();
        QString virtualization = bVirtEnabled ?
            VBoxGlobal::tr("Enabled", "details report (VT-x/AMD-V)") :
            VBoxGlobal::tr("Disabled", "details report (VT-x/AMD-V)");

        bool bNestEnabled = debugger.GetHWVirtExNestedPagingEnabled();
        QString nestedPaging = bNestEnabled ?
            VBoxGlobal::tr("Enabled", "nested paging") :
            VBoxGlobal::tr("Disabled", "nested paging");

        QString strCPUExecCap = QString::number(console.GetMachine().GetCPUExecutionCap());

        QString tip(QApplication::translate("UIIndicatorsPool",
                                            "Indicates the status of different "
                                            "features used by this virtual machine:"
                                            "<br><nobr><b>%1:</b>&nbsp;%2</nobr>"
                                            "<br><nobr><b>%3:</b>&nbsp;%4</nobr>"
                                            "<br><nobr><b>%5:</b>&nbsp;%6%</nobr>",
                                            "Virtualization Stuff LED")
                    .arg(VBoxGlobal::tr("VT-x/AMD-V", "details report"), virtualization)
                    .arg(VBoxGlobal::tr("Nested Paging"), nestedPaging)
                    .arg(VBoxGlobal::tr("Execution Cap", "details report"), strCPUExecCap));

        int cpuCount = console.GetMachine().GetCPUCount();
        if (cpuCount > 1)
            tip += QApplication::translate("UIIndicatorsPool", "<br><nobr><b>%1:</b>&nbsp;%2</nobr>", "Virtualization Stuff LED")
                      .arg(VBoxGlobal::tr("Processor(s)", "details report")).arg(cpuCount);

        setToolTip(tip);
        setState(bVirtEnabled);
    }

protected:
    /* For compatibility reason we do it here, later this should be moved to
     * QIStateIndicator. */
    CSession &m_session;
};

class UIIndicatorMouse : public QIWithRetranslateUI<QIStateIndicator>
{
    Q_OBJECT;

public:

    UIIndicatorMouse(CSession &session)
      : QIWithRetranslateUI<QIStateIndicator>()
      , m_session(session)
    {
        setStateIcon(0, QPixmap(":/mouse_disabled_16px.png"));
        setStateIcon(1, QPixmap(":/mouse_16px.png"));
        setStateIcon(2, QPixmap(":/mouse_seamless_16px.png"));
        setStateIcon(3, QPixmap(":/mouse_can_seamless_16px.png"));
        setStateIcon(4, QPixmap(":/mouse_can_seamless_uncaptured_16px.png"));

        retranslateUi();
    }

    void retranslateUi()
    {
        setToolTip(QApplication::translate("UIIndicatorsPool", "Indicates whether the host mouse pointer is captured by the guest OS:<br>"
                      "<nobr><img src=:/mouse_disabled_16px.png/>&nbsp;&nbsp;pointer is not captured</nobr><br>"
                      "<nobr><img src=:/mouse_16px.png/>&nbsp;&nbsp;pointer is captured</nobr><br>"
                      "<nobr><img src=:/mouse_seamless_16px.png/>&nbsp;&nbsp;mouse integration (MI) is On</nobr><br>"
                      "<nobr><img src=:/mouse_can_seamless_16px.png/>&nbsp;&nbsp;MI is Off, pointer is captured</nobr><br>"
                      "<nobr><img src=:/mouse_can_seamless_uncaptured_16px.png/>&nbsp;&nbsp;MI is Off, pointer is not captured</nobr><br>"
                      "Note that the mouse integration feature requires Guest Additions to be installed in the guest OS."));
    }

public slots:

    void setState(int iState)
    {
        if ((iState & UIMouseStateType_MouseAbsoluteDisabled) &&
            (iState & UIMouseStateType_MouseAbsolute) &&
            !(iState & UIMouseStateType_MouseCaptured))
        {
            QIStateIndicator::setState(4);
        }
        else
        {
            QIStateIndicator::setState(iState & (UIMouseStateType_MouseAbsolute | UIMouseStateType_MouseCaptured));
        }
    }

protected:
    /* For compatibility reason we do it here, later this should be moved to
     * QIStateIndicator. */
    CSession &m_session;
};

class UIIndicatorHostkey : public QIWithRetranslateUI<QIStateIndicator>
{
    Q_OBJECT;

public:

    UIIndicatorHostkey(CSession &session)
      : QIWithRetranslateUI<QIStateIndicator>()
      , m_session(session)
    {
        setStateIcon(0, QPixmap(":/hostkey_16px.png"));
        setStateIcon(1, QPixmap(":/hostkey_captured_16px.png"));
        setStateIcon(2, QPixmap(":/hostkey_pressed_16px.png"));
        setStateIcon(3, QPixmap(":/hostkey_captured_pressed_16px.png"));

        retranslateUi();
    }

    void retranslateUi()
    {
        setToolTip(QApplication::translate("UIIndicatorsPool", "Indicates whether the keyboard is captured by the guest OS "
                      "(<img src=:/hostkey_captured_16px.png/>) or not (<img src=:/hostkey_16px.png/>)."));
    }

protected:
    /* For compatibility reason we do it here, later this should be moved to
     * QIStateIndicator. */
    CSession &m_session;
};

UIIndicatorsPool::UIIndicatorsPool(CSession &session, QObject *pParent)
    : QObject(pParent)
    , m_session(session)
    , m_IndicatorsPool(UIIndicatorIndex_End, 0)
{
}

UIIndicatorsPool::~UIIndicatorsPool()
{
    for (int i = 0; i < m_IndicatorsPool.size(); ++i)
    {
        delete m_IndicatorsPool[i];
        m_IndicatorsPool[i] = 0;
    }
    m_IndicatorsPool.clear();
}

QIStateIndicator* UIIndicatorsPool::indicator(UIIndicatorIndex index)
{
    if (!m_IndicatorsPool.at(index))
    {
        switch (index)
        {
            case UIIndicatorIndex_HardDisks:
                m_IndicatorsPool[index] = new UIIndicatorHardDisks(m_session);
                break;
            case UIIndicatorIndex_OpticalDisks:
                m_IndicatorsPool[index] = new UIIndicatorOpticalDisks(m_session);
                break;
            case UIIndicatorIndex_FloppyDisks:
                m_IndicatorsPool[index] = new UIIndicatorFloppyDisks(m_session);
                break;
            case UIIndicatorIndex_NetworkAdapters:
                m_IndicatorsPool[index] = new UIIndicatorNetworkAdapters(m_session);
                break;
            case UIIndicatorIndex_USBDevices:
                m_IndicatorsPool[index] = new UIIndicatorUSBDevices(m_session);
                break;
            case UIIndicatorIndex_SharedFolders:
                m_IndicatorsPool[index] = new UIIndicatorSharedFolders(m_session);
                break;
            case UIIndicatorIndex_Virtualization:
                m_IndicatorsPool[index] = new UIIndicatorVirtualization(m_session);
                break;
            case UIIndicatorIndex_Mouse:
                m_IndicatorsPool[index] = new UIIndicatorMouse(m_session);
                break;
            case UIIndicatorIndex_Hostkey:
                m_IndicatorsPool[index] = new UIIndicatorHostkey(m_session);
                break;
            default:
                break;
        }
    }
    return m_IndicatorsPool.at(index);
}

#include "UIIndicatorsPool.moc"
