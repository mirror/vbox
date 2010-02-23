/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIActionsPool class implementation
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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

/* Local includes */
#include "UIIndicatorsPool.h"
#include "COMDefs.h"

class UIIndicatorHardDisks : public QIStateIndicator
{
    Q_OBJECT;

public:

    UIIndicatorHardDisks() : QIStateIndicator(KDeviceActivity_Idle)
    {
        setStateIcon(KDeviceActivity_Idle, QPixmap(":/hd_16px.png"));
        setStateIcon(KDeviceActivity_Reading, QPixmap(":/hd_read_16px.png"));
        setStateIcon(KDeviceActivity_Writing, QPixmap(":/hd_write_16px.png"));
        setStateIcon(KDeviceActivity_Null, QPixmap(":/hd_disabled_16px.png"));
    }
};

class UIIndicatorOpticalDisks : public QIStateIndicator
{
    Q_OBJECT;

public:

    UIIndicatorOpticalDisks() : QIStateIndicator(KDeviceActivity_Idle)
    {
        setStateIcon(KDeviceActivity_Idle, QPixmap(":/cd_16px.png"));
        setStateIcon(KDeviceActivity_Reading, QPixmap(":/cd_read_16px.png"));
        setStateIcon(KDeviceActivity_Writing, QPixmap(":/cd_write_16px.png"));
        setStateIcon(KDeviceActivity_Null, QPixmap(":/cd_disabled_16px.png"));
    }
};

class UIIndicatorFloppyDisks : public QIStateIndicator
{
    Q_OBJECT;

public:

    UIIndicatorFloppyDisks() : QIStateIndicator(KDeviceActivity_Idle)
    {
        setStateIcon(KDeviceActivity_Idle, QPixmap(":/fd_16px.png"));
        setStateIcon(KDeviceActivity_Reading, QPixmap(":/fd_read_16px.png"));
        setStateIcon(KDeviceActivity_Writing, QPixmap(":/fd_write_16px.png"));
        setStateIcon(KDeviceActivity_Null, QPixmap(":/fd_disabled_16px.png"));
    }
};

class UIIndicatorNetworkAdapters : public QIStateIndicator
{
    Q_OBJECT;

public:

    UIIndicatorNetworkAdapters() : QIStateIndicator(KDeviceActivity_Idle)
    {
        setStateIcon(KDeviceActivity_Idle, QPixmap(":/nw_16px.png"));
        setStateIcon(KDeviceActivity_Reading, QPixmap(":/nw_read_16px.png"));
        setStateIcon(KDeviceActivity_Writing, QPixmap(":/nw_write_16px.png"));
        setStateIcon(KDeviceActivity_Null, QPixmap(":/nw_disabled_16px.png"));
    }
};

class UIIndicatorUSBDevices : public QIStateIndicator
{
    Q_OBJECT;

public:

    UIIndicatorUSBDevices() : QIStateIndicator(KDeviceActivity_Idle)
    {
        setStateIcon(KDeviceActivity_Idle, QPixmap(":/usb_16px.png"));
        setStateIcon(KDeviceActivity_Reading, QPixmap(":/usb_read_16px.png"));
        setStateIcon(KDeviceActivity_Writing, QPixmap(":/usb_write_16px.png"));
        setStateIcon(KDeviceActivity_Null, QPixmap(":/usb_disabled_16px.png"));
    }
};

class UIIndicatorSharedFolders : public QIStateIndicator
{
    Q_OBJECT;

public:

    UIIndicatorSharedFolders() : QIStateIndicator(KDeviceActivity_Idle)
    {
        setStateIcon(KDeviceActivity_Idle, QPixmap(":/shared_folder_16px.png"));
        setStateIcon(KDeviceActivity_Reading, QPixmap(":/shared_folder_read_16px.png"));
        setStateIcon(KDeviceActivity_Writing, QPixmap(":/shared_folder_write_16px.png"));
        setStateIcon(KDeviceActivity_Null, QPixmap(":/shared_folder_disabled_16px.png"));
    }
};

class UIIndicatorVirtualization : public QIStateIndicator
{
    Q_OBJECT;

public:

    UIIndicatorVirtualization() : QIStateIndicator(0)
    {
        setStateIcon(0, QPixmap(":/vtx_amdv_disabled_16px.png"));
        setStateIcon(1, QPixmap(":/vtx_amdv_16px.png"));
    }
};

class UIIndicatorMouse : public QIStateIndicator
{
    Q_OBJECT;

public:

    UIIndicatorMouse() : QIStateIndicator(0)
    {
        setStateIcon(0, QPixmap(":/mouse_disabled_16px.png"));
        setStateIcon(1, QPixmap(":/mouse_16px.png"));
        setStateIcon(2, QPixmap(":/mouse_seamless_16px.png"));
        setStateIcon(3, QPixmap(":/mouse_can_seamless_16px.png"));
        setStateIcon(4, QPixmap(":/mouse_can_seamless_uncaptured_16px.png"));
    }
};

class UIIndicatorHostkey : public QIStateIndicator
{
    Q_OBJECT;

public:

    UIIndicatorHostkey() : QIStateIndicator(0)
    {
        setStateIcon(0, QPixmap(":/hostkey_16px.png"));
        setStateIcon(1, QPixmap(":/hostkey_captured_16px.png"));
        setStateIcon(2, QPixmap(":/hostkey_pressed_16px.png"));
        setStateIcon(3, QPixmap(":/hostkey_captured_pressed_16px.png"));
    }
};

UIIndicatorsPool::UIIndicatorsPool(QObject *pParent)
    : QObject(pParent)
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
                m_IndicatorsPool[index] = new UIIndicatorHardDisks;
                break;
            case UIIndicatorIndex_OpticalDisks:
                m_IndicatorsPool[index] = new UIIndicatorOpticalDisks;
                break;
            case UIIndicatorIndex_NetworkAdapters:
                m_IndicatorsPool[index] = new UIIndicatorNetworkAdapters;
                break;
            case UIIndicatorIndex_USBDevices:
                m_IndicatorsPool[index] = new UIIndicatorUSBDevices;
                break;
            case UIIndicatorIndex_SharedFolders:
                m_IndicatorsPool[index] = new UIIndicatorSharedFolders;
                break;
            case UIIndicatorIndex_Virtualization:
                m_IndicatorsPool[index] = new UIIndicatorVirtualization;
                break;
            case UIIndicatorIndex_Mouse:
                m_IndicatorsPool[index] = new UIIndicatorMouse;
                break;
            case UIIndicatorIndex_Hostkey:
                m_IndicatorsPool[index] = new UIIndicatorHostkey;
                break;
            default:
                break;
        }
    }
    return m_IndicatorsPool.at(index);
}

#include "UIIndicatorsPool.moc"

