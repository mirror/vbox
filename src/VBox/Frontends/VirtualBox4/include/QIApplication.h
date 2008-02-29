/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * innotek Qt extensions: QIApplication class declaration
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

#ifndef __QIApplication_h__
#define __QIApplication_h__

#include <qapplication.h>

typedef bool (*QIFilterCallback)(EventRef inEvent, void *inUserArg);


/** Sligtly modified QApplication class.
 *
 *  The sole purpose of this class (ATM) is to hook the macEventFilter
 *  in order to intercept Command-Q, Command-H and similar menu hot-keys
 *  before the HI Manager translate them into (menu) command events and
 *  start blinking menus in the menu bar.
 *
 * @remark
 *  A special hack in qeventloop_mac.cpp is required for this
 *  to work. Overloading QEventLoop::processEvents isn't feasable
 *  unfortunately, thus the horrible hacks. Qt 4 does seem to provide
 *  an interface similar to the one we create here.
 *
 *  Btw. is QI* the right right way to do this? Or should it perhapse
 *  be called VBoxQApplication or something?
 */
class QIApplication : public QApplication
{
public:
    QIApplication (int &argc, char **argv)
        : QApplication (argc, argv)
#ifdef Q_WS_MAC
        , m_callback (NULL)
        , m_callbackUserArg (NULL)
#endif
    {
    }

#ifdef Q_WS_MAC
    bool macEventFilter (EventHandlerCallRef, EventRef inEvent)
    {
        if (    m_callback
            &&  m_callback (inEvent, m_callbackUserArg))
            return true;
        return false;
    }

    void setEventFilter (QIFilterCallback callback, void *inUserArg)
    {
        m_callback = callback;
        m_callbackUserArg = inUserArg;
    }

protected:
    QIFilterCallback m_callback;
    void *m_callbackUserArg;

public:
#endif
};


#endif // __QIApplication_h__

