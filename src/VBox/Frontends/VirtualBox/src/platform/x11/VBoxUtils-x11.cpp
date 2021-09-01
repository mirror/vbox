/* $Id$ */
/** @file
 * VBox Qt GUI - Declarations of utility classes and functions for handling X11 specific tasks.
 */

/*
 * Copyright (C) 2008-2021 Oracle Corporation
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
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>
#include <QX11Info>

/* GUI includes: */
#include "VBoxUtils-x11.h"

/* Other VBox includes: */
#include <VBox/log.h>

/* Other includes: */
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/dpms.h>

/// @todo is it required still?
// WORKAROUND:
// rhel3 build hack
//RT_C_DECLS_BEGIN
//#include <X11/Xlib.h>
//#undef BOOL /* VBox/com/defs.h conflict */
//RT_C_DECLS_END


bool NativeWindowSubsystem::X11IsCompositingManagerRunning()
{
    /* For each screen it manage, compositing manager MUST acquire ownership
     * of a selection named _NET_WM_CM_Sn, where n is the screen number. */
    Display *pDisplay = QX11Info::display();
    Atom atom_property_name = XInternAtom(pDisplay, "_NET_WM_CM_S0", True);
    return XGetSelectionOwner(pDisplay, atom_property_name);
}

X11WMType NativeWindowSubsystem::X11WindowManagerType()
{
    /* Ask if root-window supports check for WM name: */
    Display *pDisplay = QX11Info::display();
    Atom atom_property_name;
    Atom atom_returned_type;
    int iReturnedFormat;
    unsigned long ulReturnedItemCount;
    unsigned long ulDummy;
    unsigned char *pcData = 0;
    X11WMType wmType = X11WMType_Unknown;
    atom_property_name = XInternAtom(pDisplay, "_NET_SUPPORTING_WM_CHECK", True);
    if (XGetWindowProperty(pDisplay, QX11Info::appRootWindow(), atom_property_name,
                           0, 512, False, XA_WINDOW, &atom_returned_type,
                           &iReturnedFormat, &ulReturnedItemCount, &ulDummy, &pcData) == Success)
    {
        Window WMWindow = None;
        if (atom_returned_type == XA_WINDOW && iReturnedFormat == 32)
            WMWindow = *((Window*)pcData);
        if (pcData)
            XFree(pcData);
        if (WMWindow != None)
        {
            /* Ask root-window for WM name: */
            atom_property_name = XInternAtom(pDisplay, "_NET_WM_NAME", True);
            Atom utf8Atom = XInternAtom(pDisplay, "UTF8_STRING", True);
            if (XGetWindowProperty(pDisplay, WMWindow, atom_property_name,
                                   0, 512, False, utf8Atom, &atom_returned_type,
                                   &iReturnedFormat, &ulReturnedItemCount, &ulDummy, &pcData) == Success)
            {
                if (QString((const char*)pcData).contains("Compiz", Qt::CaseInsensitive))
                    wmType = X11WMType_Compiz;
                else
                if (QString((const char*)pcData).contains("GNOME Shell", Qt::CaseInsensitive))
                    wmType = X11WMType_GNOMEShell;
                else
                if (QString((const char*)pcData).contains("KWin", Qt::CaseInsensitive))
                    wmType = X11WMType_KWin;
                else
                if (QString((const char*)pcData).contains("Metacity", Qt::CaseInsensitive))
                    wmType = X11WMType_Metacity;
                else
                if (QString((const char*)pcData).contains("Mutter", Qt::CaseInsensitive))
                    wmType = X11WMType_Mutter;
                else
                if (QString((const char*)pcData).contains("Xfwm4", Qt::CaseInsensitive))
                    wmType = X11WMType_Xfwm4;
                if (pcData)
                    XFree(pcData);
            }
        }
    }
    return wmType;
}

#if 0 // unused for now?
static int  gX11ScreenSaverTimeout;
static BOOL gX11ScreenSaverDpmsAvailable;
static BOOL gX11DpmsState;

void NativeWindowSubsystem::X11ScreenSaverSettingsInit()
{
    /* Init screen-save availability: */
    Display *pDisplay = QX11Info::display();
    int dummy;
    gX11ScreenSaverDpmsAvailable = DPMSQueryExtension(pDisplay, &dummy, &dummy);
}

void NativeWindowSubsystem::X11ScreenSaverSettingsSave()
{
    /* Actually this is a big mess. By default the libSDL disables the screen saver
     * during the SDL_InitSubSystem() call and restores the saved settings during
     * the SDL_QuitSubSystem() call. This mechanism can be disabled by setting the
     * environment variable SDL_VIDEO_ALLOW_SCREENSAVER to 1. However, there is a
     * known bug in the Debian libSDL: If this environment variable is set, the
     * screen saver is still disabled but the old state is not restored during
     * SDL_QuitSubSystem()! So the only solution to overcome this problem is to
     * save and restore the state prior and after each of these function calls. */

    Display *pDisplay = QX11Info::display();
    int dummy;
    CARD16 dummy2;
    XGetScreenSaver(pDisplay, &gX11ScreenSaverTimeout, &dummy, &dummy, &dummy);
    if (gX11ScreenSaverDpmsAvailable)
        DPMSInfo(pDisplay, &dummy2, &gX11DpmsState);
}

void NativeWindowSubsystem::X11ScreenSaverSettingsRestore()
{
    /* Restore screen-saver settings: */
    Display *pDisplay = QX11Info::display();
    int iTimeout, iInterval, iPreferBlank, iAllowExp;
    XGetScreenSaver(pDisplay, &iTimeout, &iInterval, &iPreferBlank, &iAllowExp);
    iTimeout = gX11ScreenSaverTimeout;
    XSetScreenSaver(pDisplay, iTimeout, iInterval, iPreferBlank, iAllowExp);
    if (gX11DpmsState && gX11ScreenSaverDpmsAvailable)
        DPMSEnable(pDisplay);
}
#endif // unused for now?

bool NativeWindowSubsystem::X11CheckExtension(const char *pExtensionName)
{
    /* Check extension: */
    Display *pDisplay = QX11Info::display();
    int major_opcode;
    int first_event;
    int first_error;
    return XQueryExtension(pDisplay, pExtensionName, &major_opcode, &first_event, &first_error);
}

bool X11CheckDBusConnection(const QDBusConnection &connection)
{
    if (!connection.isConnected())
    {
        const QDBusError lastError = connection.lastError();
        if (lastError.isValid())
        {
            LogRel(("QDBus error. Could not connect to D-Bus server: %s: %s\n",
                    lastError.name().toUtf8().constData(),
                    lastError.message().toUtf8().constData()));
        }
        else
            LogRel(("QDBus error. Could not connect to D-Bus server: Unable to load dbus libraries\n"));
        return false;
    }
    return true;
}

QStringList X11FindDBusScreenSaverServices(const QDBusConnection &connection)
{
    QStringList serviceNames;

    QDBusReply<QStringList> replyr = connection.interface()->registeredServiceNames();
    if (!replyr.isValid())
    {
        const QDBusError replyError = replyr.error();
        LogRel(("QDBus error. Could not query registered service names %s %s",
                replyError.name().toUtf8().constData(), replyError.message().toUtf8().constData()));
        return serviceNames;
    }

    for (int i = 0; i < replyr.value().size(); ++i)
    {
        const QString strServiceName = replyr.value()[i];
        if (strServiceName.contains("screensaver", Qt::CaseInsensitive))
            serviceNames << strServiceName;
    }
    if (serviceNames.isEmpty())
        LogRel(("QDBus error. No screen saver service found among registered DBus services."));

    return serviceNames;
}

bool NativeWindowSubsystem::X11CheckDBusScreenSaverServices()
{
    QDBusConnection connection = QDBusConnection::sessionBus();
    if (!X11CheckDBusConnection(connection))
        return false;

    QDBusReply<QStringList> replyr = connection.interface()->registeredServiceNames();
    if (!replyr.isValid())
    {
        const QDBusError replyError = replyr.error();
        LogRel(("QDBus error. Could not query registered service names %s %s",
                replyError.name().toUtf8().constData(), replyError.message().toUtf8().constData()));
        return false;
    }
    for (int i = 0; i < replyr.value().size(); ++i)
    {
        const QString strServiceName = replyr.value()[i];
        if (strServiceName.contains("screensaver", Qt::CaseInsensitive))
            return true;
    }
    LogRel(("QDBus error. No screen saver service found among registered DBus services."));
    return false;
}

void X11IntrospectInterfaceNode(const QDomElement &interface,
                                const QString &strServiceName,
                                QVector<X11ScreenSaverInhibitMethod*> &methods)
{
    QDomElement child = interface.firstChildElement();
    while (!child.isNull())
    {
        if (child.tagName() == "method" && child.attribute("name") == "Inhibit")
        {
            X11ScreenSaverInhibitMethod *newMethod = new X11ScreenSaverInhibitMethod;
            newMethod->m_iCookie = 0;
            newMethod->m_strServiceName = strServiceName;
            newMethod->m_strInterface = interface.attribute("name");
            newMethod->m_strPath = "/";
            newMethod->m_strPath.append(interface.attribute("name"));
            newMethod->m_strPath.replace(".", "/");
            methods.append(newMethod);
        }
        child = child.nextSiblingElement();
    }
}

void X11IntrospectServices(const QDBusConnection &connection,
                           const QString &strService,
                           const QString &strPath,
                           QVector<X11ScreenSaverInhibitMethod*> &methods)
{
    QDBusMessage call = QDBusMessage::createMethodCall(strService, strPath.isEmpty() ? QLatin1String("/") : strPath,
                                                       QLatin1String("org.freedesktop.DBus.Introspectable"),
                                                       QLatin1String("Introspect"));
    QDBusReply<QString> xmlReply = connection.call(call);

    if (!xmlReply.isValid())
        return;

    QDomDocument doc;
    doc.setContent(xmlReply);
    QDomElement node = doc.documentElement();
    QDomElement child = node.firstChildElement();
    while (!child.isNull())
    {
        if (child.tagName() == QLatin1String("node"))
        {
            QString subPath = strPath + QLatin1Char('/') + child.attribute(QLatin1String("name"));
            X11IntrospectServices(connection, strService, subPath, methods);
        }
        else if (child.tagName() == QLatin1String("interface"))
            X11IntrospectInterfaceNode(child, strService, methods);
        child = child.nextSiblingElement();
    }
}

QVector<X11ScreenSaverInhibitMethod*> NativeWindowSubsystem::X11FindDBusScrenSaverInhibitMethods()
{
    QVector<X11ScreenSaverInhibitMethod*> methods;

    QDBusConnection connection = QDBusConnection::sessionBus();
    if (!X11CheckDBusConnection(connection))
        return methods;

    foreach(const QString &strServiceName, X11FindDBusScreenSaverServices(connection))
        X11IntrospectServices(connection, strServiceName, "", methods);

    return methods;
}

void NativeWindowSubsystem::X11InhibitUninhibitScrenSaver(bool fInhibit, QVector<X11ScreenSaverInhibitMethod*> &inOutIhibitMethods)
{
    QDBusConnection connection = QDBusConnection::sessionBus();
    if (!X11CheckDBusConnection(connection))
        return;
    for (int i = 0; i < inOutIhibitMethods.size(); ++i)
    {
        QDBusInterface screenSaverInterface(inOutIhibitMethods[i]->m_strServiceName, inOutIhibitMethods[i]->m_strPath,
                                            inOutIhibitMethods[i]->m_strInterface, connection);
        if (!screenSaverInterface.isValid())
        {
            QDBusError error = screenSaverInterface.lastError();
            LogRel(("QDBus error for service %s: %s. %s\n",
                    inOutIhibitMethods[i]->m_strServiceName.toUtf8().constData(),
                    error.name().toUtf8().constData(),
                    error.message().toUtf8().constData()));
            continue;
        }
        QDBusReply<uint> reply;
        if (fInhibit)
        {
            reply = screenSaverInterface.call("Inhibit", "Oracle VirtualBox", "ScreenSaverInhibit");
            if (reply.isValid())
                inOutIhibitMethods[i]->m_iCookie = reply.value();
        }
        else
        {
            reply = screenSaverInterface.call("UnInhibit", inOutIhibitMethods[i]->m_iCookie);
        }
        if (!reply.isValid())
        {
            QDBusError error = reply.error();
            LogRel(("QDBus inhibition call error for service %s: %s. %s\n",
                    inOutIhibitMethods[i]->m_strServiceName.toUtf8().constData(),
                    error.name().toUtf8().constData(),
                    error.message().toUtf8().constData()));
        }
    }
}
