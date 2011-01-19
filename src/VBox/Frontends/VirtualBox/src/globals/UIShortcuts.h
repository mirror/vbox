/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIShortcuts class declarations
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIShortcuts_h__
#define __UIShortcuts_h__

#include "VBoxGlobal.h"

/* Qt includes */
#include <QHash>

class UIKeySequence
{
public:
    UIKeySequence() {}

    UIKeySequence(const QString &strId, const QString strKeySeq = "")
      : m_strId(strId)
      , m_strDefKeySeq(strKeySeq)
      , m_strCurKeySeq(strKeySeq) {}

    UIKeySequence(const QString &strId, QKeySequence::StandardKey seq)
      : m_strId(strId)
    {
        QKeySequence ks(seq);
        m_strDefKeySeq = m_strCurKeySeq = ks.toString();
    }

    QString id() const
    {
        return m_strId;
    }

    QString defaultKeySequence() const
    {
        return m_strDefKeySeq;
    }

    void setKeySequence(const QString& strKeySeq)
    {
        m_strCurKeySeq = strKeySeq;
    }

    QString keySequence() const
    {
        return m_strCurKeySeq;
    }

private:
    /* Private member vars */
    QString m_strId;
    QString m_strDefKeySeq;
    QString m_strCurKeySeq;
};

template <class T>
class UIShortcuts
{
public:
    static T *instance()
    {
        if (!m_pInstance)
            m_pInstance = new T();
        return m_pInstance;
    }

    static void destroy()
    {
        if (m_pInstance)
        {
            delete m_pInstance;
            m_pInstance = 0;
        }
    }

    QString shortcut(int type) const
    {
        return m_Shortcuts[type].keySequence();
    }

    QKeySequence keySequence(int type) const
    {
        return QKeySequence(m_Shortcuts[type].keySequence());
    }

protected:

    UIShortcuts() {}

    void loadExtraData(const QString &strKey, int cSize)
    {
        QStringList newKeys = vboxGlobal().virtualBox().GetExtraDataStringList(strKey);
        for (int i = 0; i < newKeys.size(); ++i)
        {
            const QString &s = newKeys.at(i);
            int w = s.indexOf('=');
            if (w < 0)
                continue;
            QString id = s.left(w);
            QString value = s.right(s.length() - w - 1);
            for (int a = 0; a < cSize; ++a)
                if (m_Shortcuts[a].id().compare(id, Qt::CaseInsensitive) == 0)
                {
                    if (value.compare("None", Qt::CaseInsensitive) == 0)
                        m_Shortcuts[a].setKeySequence("");
                    else
                        m_Shortcuts[a].setKeySequence(value);
                }
        }
    }

    static T *m_pInstance;
    QHash<int, UIKeySequence> m_Shortcuts;
};

#endif /* __UIShortcuts_h__ */

