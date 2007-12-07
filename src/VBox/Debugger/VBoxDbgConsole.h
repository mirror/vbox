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
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxDbgConsole_h__
#define __VBoxDbgConsole_h__

#include "VBoxDbgBase.h"

#include <qtextedit.h>
#include <qcombobox.h>
#include <qvbox.h>
#include <qtimer.h>

#include <iprt/critsect.h>
#include <iprt/semaphore.h>


class VBoxDbgConsoleOutput : public QTextEdit
{
    Q_OBJECT

public:
    /**
     * Constructor.
     *
     * @param   pParent     Parent Widget.
     * @param   pszName     Widget name.
     */
    VBoxDbgConsoleOutput(QWidget *pParent = NULL, const char *pszName = NULL);

    /**
     * Destructor
     */
    virtual ~VBoxDbgConsoleOutput();

    /**
     * Appends text.
     * This differs from QTextEdit::append() in that it won't start on a new paragraph
     * unless the previous char was a newline ('\n').
     *
     * @param   rStr        The text string to append.
     */
    virtual void appendText(const QString &rStr);

protected:
    /** The current line (paragraph) number. */
    unsigned m_uCurLine;
    /** The position in the current line. */
    unsigned m_uCurPos;
};


/**
 * The Debugger Console Input widget.
 *
 * This is a combobox which only responds to <return>.
 */
class VBoxDbgConsoleInput : public QComboBox
{
    Q_OBJECT

public:
    /**
     * Constructor.
     *
     * @param   pParent     Parent Widget.
     * @param   pszName     Widget name.
     */
    VBoxDbgConsoleInput(QWidget *pParent = NULL, const char *pszName = NULL);

    /**
     * Destructor
     */
    virtual ~VBoxDbgConsoleInput();

    /**
     * We overload this method to get signaled upon returnPressed().
     *
     * See QComboBox::setLineEdit for full description.
     * @param   pEdit   The new line edit widget.
     * @remark  This won't be called during the constructor.
     */
    virtual void setLineEdit(QLineEdit *pEdit);

signals:
    /**
     * New command submitted.
     */
    void commandSubmitted(const QString &rCommand);

private slots:
    /**
     * Returned was pressed.
     *
     * Will emit commandSubmitted().
     */
    void returnPressed();

protected:
    /** The current blank entry. */
    int m_iBlankItem;
};


/**
 * The Debugger Console.
 */
class VBoxDbgConsole : public QVBox, public VBoxDbgBase
{
    Q_OBJECT

public:
    /**
     * Constructor.
     *
     * @param   pVM         VM handle.
     * @param   pParent     Parent Widget.
     * @param   pszName     Widget name.
     */
    VBoxDbgConsole(PVM pVM, QWidget *pParent = NULL, const char *pszName = NULL);

    /**
     * Destructor
     */
    virtual ~VBoxDbgConsole();

protected slots:
    /**
     * Handler called when a command is submitted.
     * (Enter or return pressed in the combo box.)
     *
     * @param   rCommand        The submitted command.
     */
    void commandSubmitted(const QString &rCommand);

    /**
     * Updates the output with what's currently in the output buffer.
     * This is called by a timer or a User event posted by the debugger thread.
     */
    void updateOutput();


protected:
    /**
     * Lock the object.
     */
    void lock();

    /**
     * Unlocks the object.
     */
    void unlock();

protected:
    /** @name Debug Console Backend.
     * @{
     */


    /**
     * Checks if there is input.
     *
     * @returns true if there is input ready.
     * @returns false if there not input ready.
     * @param   pBack       Pointer to VBoxDbgConsole::m_Back.
     * @param   cMillies    Number of milliseconds to wait on input data.
     */
    static DECLCALLBACK(bool) backInput(PDBGCBACK pBack, uint32_t cMillies);

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
    static DECLCALLBACK(int) backRead(PDBGCBACK pBack, void *pvBuf, size_t cbBuf, size_t *pcbRead);

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
    static DECLCALLBACK(int) backWrite(PDBGCBACK pBack, const void *pvBuf, size_t cbBuf, size_t *pcbWritten);

    /**
     * The Debugger Console Thread
     *
     * @returns VBox status code (ignored).
     * @param   Thread      The thread handle.
     * @param   pvUser      Pointer to the VBoxDbgConsole object.s
     */
    static DECLCALLBACK(int) backThread(RTTHREAD Thread, void *pvUser);

    /** @} */

protected:
    /**
     * Use to get the GUI thread to insert the output data.
     */
    void customEvent(QCustomEvent *pEvent);

protected:
    /** The output widget. */
    VBoxDbgConsoleOutput *m_pOutput;
    /** The input widget. */
    VBoxDbgConsoleInput *m_pInput;
    /** A hack to restore focus to the combobox after a command execution. */
    bool m_fInputRestoreFocus;
    /** The input buffer. */
    char *m_pszInputBuf;
    /** The amount of input in the buffer. */
    size_t m_cbInputBuf;
    /** The allocated size of the buffer. */
    size_t m_cbInputBufAlloc;

    /** The output buffer. */
    char *m_pszOutputBuf;
    /** The amount of output in the buffer. */
    size_t m_cbOutputBuf;
    /** The allocated size of the buffer. */
    size_t m_cbOutputBufAlloc;
    /** The timer object used to process output in a delayed fashion. */
    QTimer m_Timer;
    /** Set when an output update is pending. */
    bool volatile m_fUpdatePending;

    /** The debugger console thread. */
    RTTHREAD m_Thread;
    /** The event semaphore used to signal the debug console thread about input. */
    RTSEMEVENT m_EventSem;
    /** The critical section used to lock the object. */
    RTCRITSECT m_Lock;
    /** When set the thread will cause the debug console thread to terminate. */
    bool volatile m_fTerminate;

    /** The debug console backend structure.
     * Use VBOXDBGCONSOLE_FROM_DBGCBACK to convert the DBGCBACK pointer to a object pointer. */
    struct VBoxDbgConsoleBack
    {
        DBGCBACK Core;
        VBoxDbgConsole *pSelf;
    } m_Back;

    /**
     * Converts a pointer to VBoxDbgConsole::m_Back to VBoxDbgConsole pointer.
     * @todo find a better way because offsetof is undefined on objects and g++ gets very noisy because of that.
     */
    #define VBOXDBGCONSOLE_FROM_DBGCBACK(pBack) ( ((struct VBoxDbgConsoleBack *)(pBack))->pSelf )
};


#endif
