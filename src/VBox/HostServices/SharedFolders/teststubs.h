/** @file
 * VBox Shared Folders testcase stub redefinitions.
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

/**
 * Macros for renaming iprt file operations to redirect them to testcase
 * stub functions (mocks).  The religiously correct way to do this would be
 * to make the service use a file operations structure with function pointers
 * but I'm not sure that would be universally appreciated. */

#ifndef __VBSF_TEST_STUBS__H
#define __VBSF_TEST_STUBS__H

#define RTDirClose           testRTDirClose
#define RTDirCreate          testRTDirCreate
#define RTDirOpen            testRTDirOpen
#define RTDirQueryInfo       testRTDirQueryInfo
#define RTDirRemove          testRTDirRemove
#define RTDirReadEx          testRTDirReadEx
#define RTDirSetTimes        testRTDirSetTimes
#define RTFileClose          testRTFileClose
#define RTFileDelete         testRTFileDelete
#define RTFileFlush          testRTFileFlush
#define RTFileLock           testRTFileLock
#define RTFileOpen           testRTFileOpen
#define RTFileQueryInfo      testRTFileQueryInfo
#define RTFileRead           testRTFileRead
#define RTFileSetMode        testRTFileSetMode
#define RTFileSetSize        testRTFileSetSize
#define RTFileSetTimes       testRTFileSetTimes
#define RTFileSeek           testRTFileSeek
#define RTFileUnlock         testRTFileUnlock
#define RTFileWrite          testRTFileWrite
#define RTFsQueryProperties  testRTFsQueryProperties
#define RTFsQuerySerial      testRTFsQuerySerial
#define RTFsQuerySizes       testRTFsQuerySizes
#define RTPathQueryInfoEx    testRTPathQueryInfoEx
#define RTSymlinkDelete      testRTSymlinkDelete
#define RTSymlinkRead        testRTSymlinkRead

#endif /* __VBSF_TEST_STUBS__H */
