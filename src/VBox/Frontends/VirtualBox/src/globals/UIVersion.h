/* $Id$ */
/** @file
 * VBox Qt GUI - UIVersion class declaration.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIVersion_h___
#define ___UIVersion_h___
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QString>

/** Represents VirtualBox version wrapper. */
class UIVersion
{
public:

    /** Constructs default object. */
    UIVersion();
    /** Constructs object based on parsed @a strFullVersionInfo. */
    UIVersion(const QString &strFullVersionInfo);

    /** Assigns this object with value of @a another. */
    UIVersion &operator=(const UIVersion &another);

    /** Returns whether this object is valid. */
    bool isValid() const;

    /** Returns whether this object is equal to @a other. */
    bool equal(const UIVersion &other) const;
    /** Checks whether this object is equal to @a other. */
    bool operator==(const UIVersion &other) const { return equal(other); }
    /** Checks whether this object is NOT equal to @a other. */
    bool operator!=(const UIVersion &other) const { return !equal(other); }

    /** Checks whether this object is less than @a other. */
    bool operator<(const UIVersion &other) const;
    /** Checks whether this object is less or equal than @a other. */
    bool operator<=(const UIVersion &other) const;
    /** Checks whether this object is greater than @a other. */
    bool operator>(const UIVersion &other) const;
    /** Checks whether this object is greater or equal than @a other. */
    bool operator>=(const UIVersion &other) const;

    /** Returns object string representation. */
    QString toString() const;

    /** Returns the object X value. */
    int x() const { return m_x; }
    /** Returns the object Y value. */
    int y() const { return m_y; }
    /** Returns the object Z value. */
    int z() const { return m_z; }
    /** Returns the object postfix. */
    QString postfix() const { return m_strPostfix; }

    /** Defines the object @a x value. */
    void setX(int x) { m_x = x; }
    /** Defines the object @a y value. */
    void setY(int y) { m_y = y; }
    /** Defines the object @a z value. */
    void setZ(int z) { m_z = z; }
    /** Defines the object @a strPostfix. */
    void setPostfix(const QString &strPostfix) { m_strPostfix = strPostfix; }

    /** Returns effective released version guessed or hardcoded for this one version.
      * This can be even the version itself. */
    UIVersion effectiveReleasedVersion() const;

private:

    /** Holds the object X value. */
    int  m_x;
    /** Holds the object Y value. */
    int  m_y;
    /** Holds the object Z value. */
    int  m_z;

    /** Holds the object postfix. */
    QString  m_strPostfix;
};

#endif /* !___UIVersion_h___ */
