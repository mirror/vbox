-- $Id$
--- @file
-- VBox Test Manager Database - Creates a new VcsBugReferences table.
--

--
-- Copyright (C) 2020 Oracle Corporation
--
-- This file is part of VirtualBox Open Source Edition (OSE), as
-- available from http://www.virtualbox.org. This file is free software;
-- you can redistribute it and/or modify it under the terms of the GNU
-- General Public License (GPL) as published by the Free Software
-- Foundation, in version 2 as it comes in the "COPYING" file of the
-- VirtualBox OSE distribution. VirtualBox OSE is distributed in the
-- hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
--
-- The contents of this file may alternatively be used under the terms
-- of the Common Development and Distribution License Version 1.0
-- (CDDL) only, as it comes in the "COPYING.CDDL" file of the
-- VirtualBox OSE distribution, in which case the provisions of the
-- CDDL are applicable instead of those of the GPL.
--
-- You may elect to license modified versions of this file under the
-- terms and conditions of either the GPL or the CDDL or both.
--

-- Die on error from now on.
\set ON_ERROR_STOP 1
\set AUTOCOMMIT 0

--
-- Create the new VcsBugReferences table.
--
CREATE TABLE VcsBugReferences (
    --- The version control tree name.
    sRepository         TEXT        NOT NULL,
    --- The version control tree revision number.
    iRevision           INTEGER     NOT NULL,
    --- The bug tracker identifier - see g_kaBugTrackers in config.py.
    sBugTracker         CHAR(4)     NOT NULL,
    --- The bug number in the bug tracker.
    lBugNo              BIGINT      NOT NULL,

    UNIQUE (sRepository, iRevision, sBugTracker, lBugNo)
);
CREATE INDEX VcsBugReferencesLookupIdx ON VcsBugReferences (sBugTracker, lBugNo);
COMMIT;
\d+ VcsBugReferences;

