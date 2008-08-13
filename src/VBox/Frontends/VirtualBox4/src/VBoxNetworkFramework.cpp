/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxNetworkFramework class implementation
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

#include <VBoxNetworkFramework.h>

/* Qt includes */
#include <QApplication>

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
        , mData (aData, aSize) {}

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
            mDataStream.writeRawData (e->mData.data(), e->mData.size());
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
                                        const QString &aUrl,
                                        const QString &aBody,
                                        const QStringList &aHeaders)
{
    /* Network requests thread class */
    class Thread : public QThread
    {
    public:

        Thread (QObject *aHandler, const QString &aHost, const QString &aUrl,
                const QString &aBody, const QStringList &aHeaders)
            : mHandler (aHandler), mHost (aHost), mUrl (aUrl)
            , mBody (aBody), mHeaders (aHeaders) {}

        virtual void run()
        {
            try
            {
                /* Create & setup connection */
                HConnect conn (mHost.toAscii().constData(), 80);
                conn.setcallbacks (onBegin, onData, onFinish, mHandler);

                /* Format POST request */
                conn.putrequest ("POST", mUrl.toAscii().constData());

                /* Append standard headers */
                conn.putheader ("Connection", "close");
                conn.putheader ("Content-Length", mBody.size());
                conn.putheader ("Content-type", "application/x-www-form-urlencoded");
                conn.putheader ("Accept", "text/plain");

                /* Append additional headers */
                for (int i = 0; i < mHeaders.size(); i = i + 2)
                    conn.putheader (mHeaders [i].toAscii().constData(),
                                    mHeaders [i + 1].toAscii().constData());

                /* Finishing header */
                conn.endheaders();

                /* Append & send body */
                conn.send ((const unsigned char*) mBody.toAscii().constData(),
                           mBody.toAscii().size());

                /* Pull the connection for response */
                while (conn.outstanding())
                    conn.pump();
            }
            catch (happyhttp::Wobbly &ex)
            {
                QApplication::postEvent (mHandler, new PostErrorEvent (ex.what()));
            }
        }

    private:

        QObject *mHandler;
        QString mHost;
        QString mUrl;
        QString mBody;
        QStringList mHeaders;
    };

    if (mNetworkThread)
        mNetworkThread->wait (1000);
    delete mNetworkThread;
    mNetworkThread = new Thread (this, aHost, aUrl, aBody, aHeaders);
    mNetworkThread->start();
}

