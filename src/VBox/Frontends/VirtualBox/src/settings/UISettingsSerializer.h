/** @file
 * VBox Qt GUI - UISettingsSerializer class declaration.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UISettingsSerializer_h___
#define ___UISettingsSerializer_h___

/* Qt includes: */
#include <QThread>
#include <QVariant>
#include <QWaitCondition>
#include <QMutex>
#include <QList>
#include <QMap>

/* Forward declarations: */
class UISettingsPage;

/* Type definitions: */
typedef QList<UISettingsPage*> UISettingsPageList;
typedef QMap<int, UISettingsPage*> UISettingsPageMap;

/** QThread reimplementation used for
  * loading/saving settings in async mode. */
class UISettingsSerializer : public QThread
{
    Q_OBJECT;

signals:

    /** Notifies GUI thread about process has been started. */
    void sigNotifyAboutProcessStarted();

    /** Notifies GUI thread about some page was processed. */
    void sigNotifyAboutPageProcessed(int iPageId);

    /** Notifies GUI thread about all pages were processed. */
    void sigNotifyAboutPagesProcessed();

public:

    /** Serialization directions. */
    enum SerializationDirection { Load, Save };

    /** Returns the singleton instance. */
    static UISettingsSerializer* instance() { return m_spInstance; }

    /** Constructor.
      * @param pParent   being passed to the base-class,
      * @param direction determines the load/save direction,
      * @param data      contains the wrapper(s) to load/save the data from/to,
      * @param pages     contains the page(s) to load/save the data to/from. */
    UISettingsSerializer(QObject *pParent, SerializationDirection direction,
                         const QVariant &data, const UISettingsPageList &pages);

    /** Destructor. */
    ~UISettingsSerializer();

    /** Returns the instance of wrapper(s) to load/save the data from/to. */
    QVariant& data() { return m_data; }

    /** Raises the priority of page with @a iPageId. */
    void raisePriorityOfPage(int iPageId);

public slots:

    /** Starts the process of data loading with passed @a priority. */
    void start(Priority priority = InheritPriority);

protected slots:

    /** Handles the fact of page with @a iPageId was processed. */
    void sltHandleProcessedPage(int iPageId);

    /** Handles the fact of all pages were processed. */
    void sltHandleProcessedPages();

    /** Killing serializer, softly :) */
    void sltDestroySerializer();

protected:

    /** Worker-thread serialization rutine. */
    void run();

    /** Holds the singleton instance. */
    static UISettingsSerializer *m_spInstance;

    /** Holds the the load/save direction. */
    SerializationDirection m_direction;

    /** Holds the wrapper(s) to load/save the data from/to. */
    QVariant m_data;
    /** Holds the page(s) to load/save the data to/from. */
    UISettingsPageMap m_pages;

    /** Holds whether the save was complete. */
    bool m_fSavingComplete;
    /** Holds whether it is allowed to destroy the serializer. */
    bool m_fAllowToDestroySerializer;
    /** Holds the ID of the high priority page. */
    int m_iIdOfHighPriorityPage;
    /** Holds the synchronization mutex. */
    QMutex m_mutex;
    /** Holds the synchronization condition. */
    QWaitCondition m_condition;
};

#endif /* !___UISettingsSerializer_h___ */
