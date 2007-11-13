/** @file
 *
 * VBox Debugger GUI - Console.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "VBoxDbgConsole.h"

#include <qlabel.h>
#include <qapplication.h>
#include <qfont.h>
#include <qtextview.h>
#include <qlineedit.h>

#include <VBox/dbg.h>
#include <VBox/cfgm.h>
#include <VBox/err.h>

#include <iprt/thread.h>
#include <iprt/tcp.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/alloc.h>
#include <iprt/string.h>




/*
 *
 *          V B o x D b g C o n s o l e O u t p u t
 *          V B o x D b g C o n s o l e O u t p u t
 *          V B o x D b g C o n s o l e O u t p u t
 *
 *
 */


VBoxDbgConsoleOutput::VBoxDbgConsoleOutput(QWidget *pParent/* = NULL*/, const char *pszName/* = NULL*/)
    : QTextEdit(pParent, pszName), m_uCurLine(0), m_uCurPos(0)
{
    setReadOnly(true);
    setUndoRedoEnabled(false);
    setOverwriteMode(true);
    setTextFormat(PlainText);   /* minimal HTML: setTextFormat(LogText); */

    QFont Font = font();
    Font.setStyleHint(QFont::TypeWriter);
    Font.setFamily("Courier [Monotype]");
    setFont(Font);
}

VBoxDbgConsoleOutput::~VBoxDbgConsoleOutput()
{
}

void VBoxDbgConsoleOutput::appendText(const QString &rStr)
{
    if (rStr.isEmpty() || rStr.isNull() || !rStr.length())
        return;

    /*
     * Insert line by line.
     */
    unsigned cch = rStr.length();
    unsigned iPos = 0;
    while (iPos < cch)
    {
        int iPosNL = rStr.find('\n', iPos);
        int iPosEnd = iPosNL >= 0 ? iPosNL : cch;
        if ((unsigned)iPosNL != iPos)
        {
            QString Str = rStr.mid(iPos, iPosEnd - iPos);
            if (m_uCurPos == 0)
                append(Str);
            else
                insertAt(Str, m_uCurLine, m_uCurPos);
            if (iPosNL >= 0)
            {
                m_uCurLine++;
                m_uCurPos = 0;
            }
            else
                m_uCurPos += Str.length();
        }
        else
        {
            m_uCurLine++;
            m_uCurPos = 0;
        }
        /* next */
        iPos = iPosEnd + 1;
    }
}




/*
 *
 *      V B o x D b g C o n s o l e I n p u t
 *      V B o x D b g C o n s o l e I n p u t
 *      V B o x D b g C o n s o l e I n p u t
 *
 *
 */


VBoxDbgConsoleInput::VBoxDbgConsoleInput(QWidget *pParent/* = NULL*/, const char *pszName/* = NULL*/)
    : QComboBox(true, pParent, pszName), m_iBlankItem(0)
{
    insertItem("", m_iBlankItem);
    //setInsertionPolicy(AfterCurrent);
    setInsertionPolicy(NoInsertion);
    setMaxCount(50);
    const QLineEdit *pEdit = lineEdit();
    if (pEdit)
        connect(pEdit, SIGNAL(returnPressed()), this, SLOT(returnPressed()));
}

VBoxDbgConsoleInput::~VBoxDbgConsoleInput()
{
}

void VBoxDbgConsoleInput::setLineEdit(QLineEdit *pEdit)
{
    QComboBox::setLineEdit(pEdit);
    if (lineEdit() == pEdit && pEdit)
        connect(pEdit, SIGNAL(returnPressed()), this, SLOT(returnPressed()));
}

void VBoxDbgConsoleInput::returnPressed()
{
    /* deal with the current command. */
    QString Str = currentText();
    emit commandSubmitted(Str);

    /* update the history and clear the entry field */
    if (text(m_iBlankItem - 1) != Str)
    {
        changeItem(Str, m_iBlankItem);
        removeItem(m_iBlankItem - maxCount() - 1);
        insertItem("", ++m_iBlankItem);
    }

    clearEdit();
    setCurrentItem(m_iBlankItem);
}






/*
 *
 *      V B o x D b g C o n s o l e
 *      V B o x D b g C o n s o l e
 *      V B o x D b g C o n s o l e
 *
 *
 */


VBoxDbgConsole::VBoxDbgConsole(PVM pVM, QWidget *pParent/* = NULL*/, const char *pszName/* = NULL*/)
    : VBoxDbgBase(pVM), m_pOutput(NULL), m_pInput(NULL),
    m_pszInputBuf(NULL), m_cbInputBuf(0), m_cbInputBufAlloc(0),
    m_pszOutputBuf(NULL), m_cbOutputBuf(0), m_cbOutputBufAlloc(0),
    m_Timer(), m_fUpdatePending(false), m_Thread(NIL_RTTHREAD), m_EventSem(NIL_RTSEMEVENT), m_fTerminate(false)
{
    setCaption("VBoxDbg - Console");

    NOREF(pszName);
    NOREF(pParent);

    /*
     * Create the output text box.
     */
    m_pOutput = new VBoxDbgConsoleOutput(this);

    /* try figure a suitable size */
    QLabel *pLabel = new QLabel(NULL, "11111111111111111111111111111111111111111111111111111111111111111111111111111112222222222", this);
    pLabel->setFont(m_pOutput->font());
    QSize Size = pLabel->sizeHint();
    delete pLabel;
    Size.setWidth((int)(Size.width() * 1.10));
    Size.setHeight(Size.width() / 2);
    resize(Size);

    /*
     * Create the input combo box (with a label).
     */
    QHBox *pHBox = new QHBox(this);

    pLabel = new QLabel(NULL, " Command ", pHBox);
    pLabel->setMaximumSize(pLabel->sizeHint());
    pLabel->setAlignment(AlignHCenter | AlignVCenter);

    m_pInput = new VBoxDbgConsoleInput(pHBox);
    m_pInput->setDuplicatesEnabled(false);
    connect(m_pInput, SIGNAL(commandSubmitted(const QString &)), this, SLOT(commandSubmitted(const QString &)));

    /*
     * The tab order is from input to output, not the otherway around as it is by default.
     */
    setTabOrder(m_pInput, m_pOutput);

    /*
     * Init the backend structure.
     */
    m_Back.Core.pfnInput = backInput;
    m_Back.Core.pfnRead  = backRead;
    m_Back.Core.pfnWrite = backWrite;
    m_Back.pSelf = this;

    /*
     * Create the critical section, the event semaphore and the debug console thread.
     */
    int rc = RTCritSectInit(&m_Lock);
    AssertRC(rc);

    rc = RTSemEventCreate(&m_EventSem);
    AssertRC(rc);

    rc = RTThreadCreate(&m_Thread, backThread, this, 0, RTTHREADTYPE_DEBUGGER, RTTHREADFLAGS_WAITABLE, "VBoxDbgC");
    AssertRC(rc);
    if (VBOX_FAILURE(rc))
        m_Thread = NIL_RTTHREAD;
}

VBoxDbgConsole::~VBoxDbgConsole()
{
    /*
     * Wait for the thread.
     */
    ASMAtomicXchgSize(&m_fTerminate, true);
    RTSemEventSignal(m_EventSem);
    if (m_Thread != NIL_RTTHREAD)
    {
        int rc = RTThreadWait(m_Thread, 15000, NULL);
        AssertRC(rc);
        m_Thread = NIL_RTTHREAD;
    }

    /*
     * Free resources.
     */
    RTCritSectDelete(&m_Lock);
    RTSemEventDestroy(m_EventSem);
    m_EventSem = 0;
    m_pOutput = NULL;
    m_pInput = NULL;
    if (m_pszInputBuf)
    {
        RTMemFree(m_pszInputBuf);
        m_pszInputBuf = NULL;
    }
    m_cbInputBuf = 0;
    m_cbInputBufAlloc = 0;
}

void VBoxDbgConsole::commandSubmitted(const QString &rCommand)
{
    lock();
    RTSemEventSignal(m_EventSem);

    const char *psz = rCommand;//.utf8();
    size_t cb = strlen(psz);

    /*
     * Make sure we've got space for the input.
     */
    if (cb + m_cbInputBuf >= m_cbInputBufAlloc)
    {
        size_t cbNew = RT_ALIGN_Z(cb + m_cbInputBufAlloc + 1, 128);
        void *pv = RTMemRealloc(m_pszInputBuf, cbNew);
        if (!pv)
        {
            unlock();
            return;
        }
        m_pszInputBuf = (char *)pv;
        m_cbInputBufAlloc = cbNew;
    }

    /*
     * Add the input and output it.
     */
    memcpy(m_pszInputBuf + m_cbInputBuf, psz, cb);
    m_cbInputBuf += cb;
    m_pszInputBuf[m_cbInputBuf++] = '\n';

    m_pOutput->appendText(rCommand + "\n");
    m_pOutput->scrollToBottom();

    m_fInputRestoreFocus = m_pInput->hasFocus();    /* dirty focus hack */
    m_pInput->setEnabled(false);

    unlock();
}

void VBoxDbgConsole::updateOutput()
{
    lock();
    m_fUpdatePending = false;
    if (m_cbOutputBuf)
    {
        m_pOutput->appendText(QString::fromUtf8((const char *)m_pszOutputBuf, m_cbOutputBuf));
        m_cbOutputBuf = 0;
    }
    unlock();
}


/**
 * Lock the object.
 */
void VBoxDbgConsole::lock()
{
    RTCritSectEnter(&m_Lock);
}

/**
 * Unlocks the object.
 */
void VBoxDbgConsole::unlock()
{
    RTCritSectLeave(&m_Lock);
}



/**
 * Checks if there is input.
 *
 * @returns true if there is input ready.
 * @returns false if there not input ready.
 * @param   pBack       Pointer to VBoxDbgConsole::m_Back.
 * @param   cMillies    Number of milliseconds to wait on input data.
 */
/*static*/ DECLCALLBACK(bool) VBoxDbgConsole::backInput(PDBGCBACK pBack, uint32_t cMillies)
{
    VBoxDbgConsole *pThis = VBOXDBGCONSOLE_FROM_DBGCBACK(pBack);
    pThis->lock();

    /* questing for input means it's done processing. */
    pThis->m_pInput->setEnabled(true);
    /* dirty focus hack: */
    if (pThis->m_fInputRestoreFocus)
    {
        pThis->m_fInputRestoreFocus = false;
        if (!pThis->m_pInput->hasFocus())
            pThis->m_pInput->setFocus();
    }

    bool fRc = true;
    if (!pThis->m_cbInputBuf)
    {
        pThis->unlock();
        RTSemEventWait(pThis->m_EventSem, cMillies);
        pThis->lock();
        fRc = pThis->m_cbInputBuf || pThis->m_fTerminate;
    }

    pThis->unlock();
    return fRc;
}

/**
 * Read input.
 *
 * @returns VBox status code.
 * @param   pBack       Pointer to VBoxDbgConsole::m_Back.
 * @param   pvBuf       Where to put the bytes we read.
 * @param   cbBuf       Maximum nymber of bytes to read.
 * @param   pcbRead     Where to store the number of bytes actually read.
 *                      If NULL the entire buffer must be filled for a
 *                      successful return.
 */
/*static*/ DECLCALLBACK(int) VBoxDbgConsole::backRead(PDBGCBACK pBack, void *pvBuf, size_t cbBuf, size_t *pcbRead)
{
    VBoxDbgConsole *pThis = VBOXDBGCONSOLE_FROM_DBGCBACK(pBack);
    Assert(pcbRead); /** @todo implement this bit */
    if (pcbRead)
        *pcbRead = 0;

    pThis->lock();
    int rc = VINF_SUCCESS;
    if (!pThis->m_fTerminate)
    {
        if (pThis->m_cbInputBuf)
        {
            const char *psz = pThis->m_pszInputBuf;
            size_t cbRead = RT_MIN(pThis->m_cbInputBuf, cbBuf);
            memcpy(pvBuf, psz, cbRead);
            psz += cbRead;
            pThis->m_cbInputBuf -= cbRead;
            if (*psz)
                memmove(pThis->m_pszInputBuf, psz, pThis->m_cbInputBuf);
            pThis->m_pszInputBuf[pThis->m_cbInputBuf] = '\0';
            *pcbRead = cbRead;
        }
    }
    else
        rc = VERR_GENERAL_FAILURE;
    pThis->unlock();
    return rc;
}

/**
 * Write (output).
 *
 * @returns VBox status code.
 * @param   pBack       Pointer to VBoxDbgConsole::m_Back.
 * @param   pvBuf       What to write.
 * @param   cbBuf       Number of bytes to write.
 * @param   pcbWritten  Where to store the number of bytes actually written.
 *                      If NULL the entire buffer must be successfully written.
 */
/*static*/ DECLCALLBACK(int) VBoxDbgConsole::backWrite(PDBGCBACK pBack, const void *pvBuf, size_t cbBuf, size_t *pcbWritten)
{
    VBoxDbgConsole *pThis = VBOXDBGCONSOLE_FROM_DBGCBACK(pBack);
    int rc = VINF_SUCCESS;

    pThis->lock();
    if (cbBuf + pThis->m_cbOutputBuf >= pThis->m_cbOutputBufAlloc)
    {
        size_t cbNew = RT_ALIGN_Z(cbBuf + pThis->m_cbOutputBufAlloc + 1, 1024);
        void *pv = RTMemRealloc(pThis->m_pszOutputBuf, cbNew);
        if (!pv)
        {
            pThis->unlock();
            if (pcbWritten)
                *pcbWritten = 0;
            return VERR_NO_MEMORY;
        }
        pThis->m_pszOutputBuf = (char *)pv;
        pThis->m_cbOutputBufAlloc = cbNew;
    }

    /*
     * Add the output.
     */
    memcpy(pThis->m_pszOutputBuf + pThis->m_cbOutputBuf, pvBuf, cbBuf);
    pThis->m_cbOutputBuf += cbBuf;
    pThis->m_pszOutputBuf[pThis->m_cbOutputBuf] = '\0';
    if (pcbWritten)
        *pcbWritten = cbBuf;

    if (pThis->m_fTerminate)
        rc = VERR_GENERAL_FAILURE;

    /*
     * Tell the GUI thread to draw this text.
     * We cannot do it from here without frequent crashes.
     */
    if (!pThis->m_fUpdatePending)
        QApplication::postEvent(pThis, new QCustomEvent(QEvent::User, NULL));

    pThis->unlock();

    return rc;
}

/**
 * The Debugger Console Thread
 *
 * @returns VBox status code (ignored).
 * @param   Thread      The thread handle.
 * @param   pvUser      Pointer to the VBoxDbgConsole object.s
 */
/*static*/ DECLCALLBACK(int) VBoxDbgConsole::backThread(RTTHREAD Thread, void *pvUser)
{
    VBoxDbgConsole *pThis = (VBoxDbgConsole *)pvUser;
    LogFlow(("backThread: Thread=%p pvUser=%p\n", (void *)Thread, pvUser));

    NOREF(Thread);

    /*
     * Create and execute the console.
     */
    int rc = pThis->dbgcCreate(&pThis->m_Back.Core, 0);
    LogFlow(("backThread: returns %Vrc\n", rc));
    if (!pThis->m_fTerminate)
        QApplication::postEvent(pThis, new QCustomEvent(QEvent::User, (void *)1));
    return rc;
}

void VBoxDbgConsole::customEvent(QCustomEvent *pEvent)
{
    if (pEvent->type() == QEvent::User)
    {
        uintptr_t u = (uintptr_t)pEvent->data(); /** @todo enum! */
        switch (u)
        {
            /* make update pending. */
            case 0:
                if (!m_fUpdatePending)
                {
                    m_fUpdatePending = true;
                    m_Timer.singleShot(10, this, SLOT(updateOutput()));
                }
                break;

            /* the thread terminated */
            case 1:
                m_pInput->setEnabled(false);
                break;

            /* paranoia */
            default:
                AssertMsgFailed(("u=%d\n", u));
                break;
        }
    }
}

