/** @file
 *
 * VBox Debugger GUI - The Manager.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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


#define VBOX_COM_NO_ATL
#include <VBox/com/defs.h>
#include <VBox/vm.h>
#include <VBox/err.h>

#include "VBoxDbgGui.h"
#include <qdesktopwidget.h>
#include <qapplication.h>


VBoxDbgGui::VBoxDbgGui() :
    m_pDbgStats(NULL), m_pDbgConsole(NULL), m_pSession(NULL), m_pConsole(NULL),
    m_pMachineDebugger(NULL), m_pMachine(NULL), m_pVM(NULL), m_x(0), m_y(0), m_cx(0), m_cy(0),
    m_xDesktop(0), m_yDesktop(0), m_cxDesktop(0), m_cyDesktop(0)
{

}


int VBoxDbgGui::init(ISession *pSession)
{
    /*
     * Update the desktop size first.
     */
    updateDesktopSize();

    /*
     * Query the Virtual Box interfaces.
     */
    m_pSession = pSession;
    m_pSession->AddRef();

    HRESULT hrc = m_pSession->COMGETTER(Machine)(&m_pMachine);
    if (SUCCEEDED(hrc))
    {
        hrc = m_pSession->COMGETTER(Console)(&m_pConsole);
        if (SUCCEEDED(hrc))
        {
            hrc = m_pConsole->COMGETTER(Debugger)(&m_pMachineDebugger);
            if (SUCCEEDED(hrc))
            {
                /*
                 * Get the VM handle.
                 */
                ULONG64 ullVM;
                hrc = m_pMachineDebugger->COMGETTER(VM)(&ullVM);
                if (SUCCEEDED(hrc))
                {
                    m_pVM = (PVM)(uintptr_t)ullVM;
                    return VINF_SUCCESS;
                }

                /* damn, failure! */
                m_pMachineDebugger->Release();
            }
            m_pConsole->Release();
        }
        m_pMachine->Release();
    }

    return VERR_GENERAL_FAILURE;
}


VBoxDbgGui::~VBoxDbgGui()
{

    if (m_pDbgStats)
    {
        delete m_pDbgStats;
        m_pDbgStats = NULL;
    }

    if (m_pDbgConsole)
    {
        delete m_pDbgConsole;
        m_pDbgConsole = NULL;
    }

    if (m_pMachineDebugger)
    {
        m_pMachineDebugger->Release();
        m_pMachineDebugger = NULL;
    }

    if (m_pConsole)
    {
        m_pConsole->Release();
        m_pConsole = NULL;
    }

    if (m_pMachine)
    {
        m_pMachine->Release();
        m_pMachine = NULL;
    }

    if (m_pSession)
    {
        m_pSession->Release();
        m_pSession = NULL;
    }

    m_pVM = NULL;
}


int VBoxDbgGui::showStatistics()
{
    if (!m_pDbgStats)
    {
        m_pDbgStats = new VBoxDbgStats(m_pVM);
        connect(m_pDbgStats, SIGNAL(destroyed(QObject *)), this, SLOT(notifyChildDestroyed(QObject *)));
        repositionStatistics();
    }
    m_pDbgStats->show();
    return VINF_SUCCESS;
}

void VBoxDbgGui::repositionStatistics(bool fResize/* = true*/)
{
    if (m_pDbgStats)
    {
        /* Move it to the right side of the VBox console. */
        m_pDbgStats->move(m_x + m_cx, m_y);
        if (fResize)
            /* Resize it to cover all the space to the left side of the desktop. */
            resizeWidget(m_pDbgStats, m_cxDesktop - m_cx - m_x + m_xDesktop, m_cyDesktop - m_y + m_yDesktop);
    }
}


int VBoxDbgGui::showConsole()
{
    if (!m_pDbgConsole)
    {
        m_pDbgConsole = new VBoxDbgConsole(m_pVM);
        connect(m_pDbgConsole, SIGNAL(destroyed(QObject *)), this, SLOT(notifyChildDestroyed(QObject *)));
        repositionConsole();
    }
    m_pDbgConsole->show();
    return VINF_SUCCESS;
}


void VBoxDbgGui::repositionConsole(bool fResize/* = true*/)
{
    if (m_pDbgConsole)
    {
        /* Move it to the bottom of the VBox console. */
        m_pDbgConsole->move(m_x, m_y + m_cy);
        if (fResize)
            /* Resize it to cover the space down to the bottom of the desktop. */
            resizeWidget(m_pDbgConsole, m_cx, m_cyDesktop - m_cy - m_y + m_yDesktop);
    }
}


void VBoxDbgGui::updateDesktopSize()
{
    QRect Rct(0, 0, 1600, 1200);
    QDesktopWidget *pDesktop = QApplication::desktop();
    if (pDesktop)
        Rct = pDesktop->availableGeometry(QPoint(m_x, m_y));
    m_xDesktop = Rct.x();
    m_yDesktop = Rct.y();
    m_cxDesktop = Rct.width();
    m_cyDesktop = Rct.height();
}


void VBoxDbgGui::adjustRelativePos(int x, int y, unsigned cx, unsigned cy)
{
    const bool fResize = cx != m_cx || cy != m_cy;
    const bool fMoved  = x  != m_x  || y  != m_y;

    m_x = x;
    m_y = y;
    m_cx = cx;
    m_cy = cy;

    if (fMoved)
        updateDesktopSize();
    repositionConsole(fResize);
    repositionStatistics(fResize);
}


/*static*/ void VBoxDbgGui::resizeWidget(QWidget *pWidget, unsigned cx, unsigned cy)
{
    QSize FrameSize = pWidget->frameSize();
    QSize WidgetSize = pWidget->size();
    pWidget->resize(cx - (FrameSize.width() - WidgetSize.width()),
                    cy - (FrameSize.height() - WidgetSize.height()));
}

void VBoxDbgGui::notifyChildDestroyed(QObject *pObj)
{
    if (m_pDbgStats == pObj)
        m_pDbgStats = NULL;
    else if (m_pDbgConsole == pObj)
        m_pDbgConsole = NULL;
}

