/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxNetworkFramework class implementation
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

#include <VBoxNetworkFramework.h>
#include <qapplication.h>
//Added by qt3to4:
#include <QEvent>

/* These notifications are used to notify the GUI thread about different
 * downloading events: Downloading Started, Downloading in Progress,
 * Downloading Finished, Downloading Error. */
enum PostingEvents
{
    PostBeginEventType = QEvent::User + 500,
    PostDataEventType,
    PostFinishEventType,
    PostErrorEventType
};

class PostBeginEvent : public QEvent
{
public:
    PostBeginEvent (int aStatus)
        : QEvent ((QEvent::Type) PostBeginEventType)
        , mStatus (aStatus) {}

    int  mStatus;
};

class PostDataEvent : public QEvent
{
public:
    PostDataEvent (const char *aData, ulong aSize)
        : QEvent ((QEvent::Type) PostDataEventType)
        , mData (QByteArray().duplicate (aData, aSize)) {}

    QByteArray mData;
};

class PostFinishEvent : public QEvent
{
public:
    PostFinishEvent()
        : QEvent ((QEvent::Type) PostFinishEventType) {}
};

class PostErrorEvent : public QEvent
{
public:
    PostErrorEvent (const QString &aInfo)
        : QEvent ((QEvent::Type) PostErrorEventType)
        , mInfo (aInfo) {}

    QString mInfo;
};

/* This callback is used to handle the request procedure beginning. */
void onBegin (const happyhttp::Response *aResponse, void *aUserdata)
{
    VBoxNetworkFramework *obj = static_cast<VBoxNetworkFramework*> (aUserdata);
    QApplication::postEvent (obj, new PostBeginEvent (aResponse->getstatus()));
}

/* This callback is used to handle the progress of request procedure. */
void onData (const happyhttp::Response*, void *aUserdata,
             const unsigned char *aData, int aSize)
{
    VBoxNetworkFramework *obj = static_cast<VBoxNetworkFramework*> (aUserdata);
    QApplication::postEvent (obj, new PostDataEvent ((const char*) aData, aSize));
}

/* This callback is used to handle the finish event of every request. */
void onFinish (const happyhttp::Response*, void *aUserdata)
{
    VBoxNetworkFramework *obj = static_cast<VBoxNetworkFramework*> (aUserdata);
    QApplication::postEvent (obj, new PostFinishEvent());
}

bool VBoxNetworkFramework::event (QEvent *aEvent)
{
    switch (aEvent->type())
    {
        case PostBeginEventType:
        {
            PostBeginEvent *e = static_cast<PostBeginEvent*> (aEvent);
            emit netBegin (e->mStatus);
            return true;
        }
        case PostDataEventType:
        {
            PostDataEvent *e = static_cast<PostDataEvent*> (aEvent);
            mDataStream.writeRawBytes (e->mData.data(), e->mData.size());
            emit netData (e->mData);
            return true;
        }
        case PostFinishEventType:
        {
            emit netEnd (mDataArray);
            return true;
        }
        case PostErrorEventType:
        {
            PostErrorEvent *e = static_cast<PostErrorEvent*> (aEvent);
            emit netError (e->mInfo);
            return true;
        }
        default:
            break;
    }

    return QObject::event (aEvent);
}

void VBoxNetworkFramework::postRequest (const QString &aHost,
                                        const QString &aUrl)
{
    /* Network requests thread class */
    class Thread : public QThread
    {
    public:

        Thread (QObject *aProc, const QString &aHost, const QString &aUrl)
            : mProc (aProc), mHost (aHost), mUrl (aUrl) {}

        virtual void run()
        {
            try
            {
                HConnect conn (mHost, 80);
                conn.setcallbacks (onBegin, onData, onFinish, mProc);
                const char *headers[] =
                {
                    "Connection", "close",
                    "Content-type", "application/x-www-form-urlencoded",
                    "Accept", "text/plain",
                    0
                };

                conn.request ("POST", mUrl.ascii(), headers, 0, 0);
                while (conn.outstanding())
                    conn.pump();
            }
            catch (happyhttp::Wobbly &ex)
            {
                QApplication::postEvent (mProc, new PostErrorEvent (ex.what()));
            }
        }

    private:

        QObject  *mProc;
        QString   mHost;
        QString   mUrl;
    };

    delete mNetworkThread;
    mNetworkThread = new Thread (this, aHost, aUrl);
    mNetworkThread->start();
}

