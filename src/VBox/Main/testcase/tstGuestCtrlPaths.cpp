/* $Id */
/** @file
 * Guest Control path test cases.
 */

/*
 * Copyright (C) 2022 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#define LOG_ENABLED
#define LOG_GROUP LOG_GROUP_MAIN
#include <VBox/log.h>

#include "../include/GuestCtrlImplPrivate.h"

using namespace com;

#include <iprt/env.h>
#include <iprt/rand.h>
#include <iprt/stream.h>
#include <iprt/test.h>


DECLINLINE(void) tstPathTranslate(Utf8Str strPath, PathStyle_T enmSrcPathStyle, PathStyle_T enmDstPathStyle, int rcExp, Utf8Str strPathExp)
{
    Utf8Str strPath2 = strPath; \
    int vrc = GuestPath::Translate(strPath2, enmSrcPathStyle, enmDstPathStyle);
    RTTESTI_CHECK_MSG_RETV(vrc == rcExp, ("Expected %Rrc, got %Rrc for '%s'\n", rcExp, vrc, strPath.c_str()));
    RTTESTI_CHECK_MSG_RETV(strPath2 == strPathExp, ("Expected '%s', got '%s'\n", strPathExp.c_str(), strPath2.c_str()));
}

int main()
{
    RTTEST hTest;
    int vrc = RTTestInitAndCreate("tstGuestCtrlPaths", &hTest);
    if (vrc)
        return vrc;
    RTTestBanner(hTest);

    RTTestIPrintf(RTTESTLVL_DEBUG, "Initializing COM...\n");
    HRESULT hrc = com::Initialize();
    if (FAILED(hrc))
    {
        RTTestFailed(hTest, "Failed to initialize COM (%Rhrc)!\n", hrc);
        return RTEXITCODE_FAILURE;
    }

    /* Don't let the assertions trigger here
     * -- we rely on the return values in the test(s) below. */
    RTAssertSetQuiet(true);

    tstPathTranslate("",    PathStyle_DOS,  PathStyle_DOS,  VINF_SUCCESS, "");

    tstPathTranslate("foo", PathStyle_DOS,  PathStyle_DOS,  VINF_SUCCESS, "foo");
    tstPathTranslate("foo", PathStyle_UNIX, PathStyle_UNIX, VINF_SUCCESS, "foo");
    tstPathTranslate("foo", PathStyle_DOS,  PathStyle_UNIX, VINF_SUCCESS, "foo");
    tstPathTranslate("foo", PathStyle_UNIX, PathStyle_UNIX, VINF_SUCCESS, "foo");

    tstPathTranslate("foo\\bar", PathStyle_DOS,  PathStyle_DOS,  VINF_SUCCESS, "foo\\bar");
    tstPathTranslate("foo/bar",  PathStyle_UNIX, PathStyle_UNIX, VINF_SUCCESS, "foo/bar");

    tstPathTranslate("foo\\bar\\", PathStyle_DOS,  PathStyle_DOS,  VINF_SUCCESS, "foo\\bar\\");
    tstPathTranslate("foo/bar/",   PathStyle_UNIX, PathStyle_UNIX, VINF_SUCCESS, "foo/bar/");
    /* Actually also allowed on Windows. */
    tstPathTranslate("foo/bar/",   PathStyle_DOS,  PathStyle_UNIX,  VINF_SUCCESS, "foo/bar/");

    tstPathTranslate("foo\\bar\\BAZ", PathStyle_DOS,  PathStyle_DOS,  VINF_SUCCESS, "foo\\bar\\BAZ");
    tstPathTranslate("foo/bar/BAZ",   PathStyle_UNIX, PathStyle_UNIX, VINF_SUCCESS, "foo/bar/BAZ");

    tstPathTranslate("foo\\bar\\dir with space\\", PathStyle_DOS,  PathStyle_UNIX, VINF_SUCCESS, "foo/bar/dir with space/");
    tstPathTranslate("foo/bar/dir with space/",    PathStyle_UNIX, PathStyle_UNIX, VINF_SUCCESS, "foo/bar/dir with space/");

    tstPathTranslate("foo/bar/dir_with_escape_sequence\\ space", PathStyle_UNIX, PathStyle_UNIX, VINF_SUCCESS, "foo/bar/dir_with_escape_sequence\\ space");
    /** Do a mapping of "\", which marks an escape sequence for paths on UNIX-y OSes to DOS-based OSes (like Windows),
      * however, on DOS "\" is a path separator.  See @bugref{21095} */
    tstPathTranslate("foo/bar/dir_with_escape_sequence/the\\ space", PathStyle_UNIX,    PathStyle_DOS,  VINF_SUCCESS, "foo\\bar\\dir_with_escape_sequence\\the space");
    tstPathTranslate("foo/bar/dir_with_escape_sequence/the\\ \\ space", PathStyle_UNIX, PathStyle_DOS,  VINF_SUCCESS, "foo\\bar\\dir_with_escape_sequence\\the  space");
    tstPathTranslate("foo/bar/dir_with_escape_sequence/spaces at end\\  \\ ", PathStyle_UNIX, PathStyle_DOS,  VINF_SUCCESS, "foo\\bar\\dir_with_escape_sequence\\spaces at end   ");

    /* Filter out double slashes (cosmetic only). */
    tstPathTranslate("\\\\",         PathStyle_DOS,  PathStyle_DOS,  VINF_SUCCESS, "\\");
    tstPathTranslate("foo\\\\bar\\", PathStyle_DOS,  PathStyle_DOS,  VINF_SUCCESS, "foo\\bar\\");

    /* Mixed slashes. */
    tstPathTranslate("\\\\foo/bar\\\\baz",       PathStyle_UNIX, PathStyle_UNIX, VINF_SUCCESS, "\\foo/bar\\baz");
    tstPathTranslate("with spaces\\ foo/\\ bar", PathStyle_UNIX, PathStyle_DOS,  VINF_SUCCESS, "with spaces foo\\ bar");

    RTTestIPrintf(RTTESTLVL_DEBUG, "Shutting down COM...\n");
    com::Shutdown();

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

