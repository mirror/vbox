/*
 * Copyright (c) 1997-1999 by The XFree86 Project, Inc.
 */

#ifndef MOUSE_H_
#define MOUSE_H_

#include "xf86OSmouse.h"

const char * xf86MouseProtocolIDToName(MouseProtocolID id);
MouseProtocolID xf86MouseProtocolNameToID(const char *name);

#endif
