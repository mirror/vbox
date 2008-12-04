/* $Id$ */
/** @file
 * Classes for handling hardware detection under Linux.  Please feel free to
 * expand these to work for other systems (Solaris!) or to add new ones for
 * other systems.
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

#include <iprt/err.h>
#include <string>
#include <vector>

/** This should only be enabled when testing.  It causes all methods to be used
 * when probing for drives instead of stopping as soon as one method is
 * successful.  This is a global instead of a define in order to keep the test
 * code closer to the real code. */
extern bool g_testHostHardwareLinux;

/**
 * Class for probing and returning information about host DVD and floppy drives
 */
class VBoxMainDriveInfo
{
public:
    /** Structure describing a host drive */
    struct DriveInfo
    {
        /** The device node of the drive. */
        std::string mDevice;
        /** The hal unique device identifier, if available. */
        std::string mUdi;
        /** A textual description of the drive. */
        std::string mDescription;

        /** Constructors */
        DriveInfo (std::string aDevice, std::string aUdi, std::string aDescription)
            : mDevice (aDevice), mUdi (aUdi), mDescription (aDescription) {}
        DriveInfo (std::string aDevice, std::string aUdi,
                   const char *aDescription = NULL)
            : mDevice (aDevice), mUdi (aUdi),
            mDescription (aDescription != NULL ? aDescription : std::string ()) {}
        DriveInfo (std::string aDevice, const char *aUdi = NULL,
                   const char *aDescription = NULL)
            : mDevice (aDevice), mUdi (aUdi != NULL ? aUdi : std::string ()),
            mDescription (aDescription != NULL ? aDescription : std::string ()) {}
    };
    
    /** List (resp vector) holding drive information */
    typedef std::vector <DriveInfo> DriveInfoList;

    /**
     * Search for host floppy drives and rebuild the list, which remains empty
     * until the first time it is called.
     * @returns iprt status code
     */
    int updateFloppies ();

    /**
     * Search for host DVD drives and rebuild the list, which remains empty
     * until the first time it is called.
     * @returns iprt status code
     */
    int updateDVDs ();

    /** Get the first element in the list of floppy drives. */
    DriveInfoList::const_iterator FloppyBegin()
    {
        return mFloppyList.begin();
    }

    /** Get the last element in the list of floppy drives. */
    DriveInfoList::const_iterator FloppyEnd()
    {
        return mFloppyList.end();
    }

    /** Get the first element in the list of DVD drives. */
    DriveInfoList::const_iterator DVDBegin()
    {
        return mDVDList.begin();
    }

    /** Get the last element in the list of DVD drives. */
    DriveInfoList::const_iterator DVDEnd()
    {
        return mDVDList.end();
    }
private:
    /** The list of currently available floppy drives */
    DriveInfoList mFloppyList;
    /** The list of currently available DVD drives */
    DriveInfoList mDVDList;
};

typedef VBoxMainDriveInfo::DriveInfoList DriveInfoList;
typedef VBoxMainDriveInfo::DriveInfo DriveInfo;

