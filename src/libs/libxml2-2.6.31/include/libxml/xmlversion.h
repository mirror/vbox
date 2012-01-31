/*
 * Include a proper pre-generated xmlversion file.
 * Note that configuration scripts will overwrite this file using a
 * xmlversion.h.in template.
 */

#ifdef WIN32
#include <win32xmlversion.h>
#else
#include <libxml/xmlversion-default.h>
#endif
