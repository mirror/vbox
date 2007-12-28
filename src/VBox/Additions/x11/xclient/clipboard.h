/** @file
 *
 * Shared Clipboard:
 * Linux guest.
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

#ifndef __Additions_linux_clipboard_h
# define __Additions_linux_clipboard_h

extern void vboxClipboardDisconnect (void);
extern int vboxClipboardConnect (void);
extern int vboxClipboardMain (void);

#endif /* __Additions_linux_clipboard_h not defined */
