/*++

 Copyright (c) 1989 - 1999 Microsoft Corporation


 --*/

#define MINIRDR__NAME NulMRx
#define ___MINIRDR_IMPORTS_NAME (NulMRxDeviceObject->RdbssExports)

#include <ntifs.h>

#include "rx.h"

#include "nodetype.h"
#include "netevent.h"

#include <windef.h>

#include "nulmrx.h"
#include "minip.h"
#include <lmcons.h>     // from the Win32 SDK
#include "mrxglobs.h"

#ifdef VBOX
#define try __try
#define leave __leave
#define finally __finally
#define except __except
/* bird: Added this to kill warnings about using unprototyped functions.
 *       I hope it doesn't break anything... */
#include "vbsfhlp.h"

////#define VBOX_FAKE_IO
////#define VBOX_FAKE_IO_READ_WRITE
////#define VBOX_FAKE_IO_CREATE
////#define VBOX_FAKE_IO_DELETE

#endif
