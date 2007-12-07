/** @file
 *
 *  Declaration of SVC Helper Process control routines.
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

#ifndef __VBox_svchlp_h__
#define __VBox_svchlp_h__

#include "VBox/com/string.h"
#include "VBox/com/guid.h"

#include <VBox/err.h>

#include <windows.h>

struct SVCHlpMsg
{
    enum Code
    {
        Null = 0, /* no parameters */
        OK, /* no parameters */
        Error, /* Utf8Str string (may be null but must present) */
        
        CreateHostNetworkInterface = 100, /* see usage in code */
        CreateHostNetworkInterface_OK, /* see usage in code */
        RemoveHostNetworkInterface, /* see usage in code */
    };
};

class SVCHlpClient
{
public:

    SVCHlpClient();
    virtual ~SVCHlpClient();

    int create (const char *aName);
    int connect();
    int open (const char *aName);
    int close();
    
    bool isOpen() const { return mIsOpen; }
    bool isServer() const { return mIsServer; }
    const com::Utf8Str &name() const { return mName; } 

    int write (const void *aVal, size_t aLen);
    template <typename Scalar>
    int write (Scalar aVal) { return write (&aVal, sizeof (aVal)); }
    int write (const com::Utf8Str &aVal);
    int write (const com::Guid &aGuid);

    int read (void *aVal, size_t aLen);
    template <typename Scalar>
    int read (Scalar &aVal) { return read (&aVal, sizeof (aVal)); }
    int read (com::Utf8Str &aVal);
    int read (com::Guid &aGuid);

private:

    bool mIsOpen : 1;
    bool mIsServer : 1;

    HANDLE mReadEnd;
    HANDLE mWriteEnd;
    com::Utf8Str mName;
};

class SVCHlpServer : public SVCHlpClient
{
public:

    SVCHlpServer();

    int run();
};

#endif /* __VBox_svchlp_h__ */

