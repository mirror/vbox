/** @file
 *
 * VBox Host Guest Shared Memory Interface (HGSMI).
 * Host/Guest shared part.
 * Channel identifiers.
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */


#ifndef __HGSMIChannels_h__
#define __HGSMIChannels_h__


/* Each channel has an 8 bit identifier. There are a number of predefined
 * (hardcoded) channels.
 *
 * HGSMI_CH_HGSMI channel can be used to map a string channel identifier
 * to a free 16 bit numerical value. values are allocated in range
 * [HGSMI_CH_STRING_FIRST;HGSMI_CH_STRING_LAST].
 * 
 */


/* Predefined channel identifiers. Used internally by VBOX to simplify the channel setup. */
#define HGSMI_CH_RESERVED     (0x00) /* A reserved channel value. */

#define HGSMI_CH_HGSMI        (0x01) /* HGCMI: setup and configuration channel. */

#define HGSMI_CH_VBVA         (0x02) /* Graphics: VBVA. */
#define HGSMI_CH_SEAMLESS     (0x03) /* Graphics: Seamless with a single guest region. */
#define HGSMI_CH_SEAMLESS2    (0x04) /* Graphics: Seamless with separate host windows. */
#define HGSMI_CH_OPENGL       (0x05) /* Graphics: OpenGL HW acceleration. */


/* Dynamically allocated channel identifiers. */
#define HGSMI_CH_STRING_FIRST (0x20) /* The first channel index to be used for string mappings (inclusive). */
#define HGSMI_CH_STRING_LAST  (0xff) /* The last channel index for string mappings (inclusive). */


/* Check whether the channel identifier is allocated for a dynamic channel. */
#define HGSMI_IS_DYNAMIC_CHANNEL(_channel) (((uint8_t)(_channel) & 0xE0) != 0)


#endif /* !__HGSMIChannels_h__*/
