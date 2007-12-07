/*
 * Copyright (C) 2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 * available from http://www.virtualbox.org.
 */

/*
 * X11 keyboard driver
 *
 * Copyright 1993 Bob Amstadt
 * Copyright 1996 Albrecht Kleine
 * Copyright 1997 David Faure
 * Copyright 1998 Morten Welinder
 * Copyright 1998 Ulrich Weigand
 * Copyright 1999 Ove Kåven
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "keyboard.h"

#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>

#include <ctype.h>
#include <stdarg.h>
#include <string.h>

static unsigned keyc2scan[256];

/* Keyboard layout tables for guessing the current keyboard layout. */
#include "keyboard-tables.h"

/**
  * Translate a keycode in a key event to a scan code, using the lookup
  * table which we constructed earlier.
  *
  * @returns the scan code number
  * @param code the X11 key code to be looked up
  */

unsigned X11DRV_KeyEvent(KeyCode code)
{
    return keyc2scan[code];
}

/**********************************************************************
 *		X11DRV_KEYBOARD_DetectLayout
 *
 * Called from X11DRV_InitKeyboard
 *  This routine walks through the defined keyboard layouts and selects
 *  whichever matches most closely.
 *
 * @returns an index into the table of keyboard layouts, or 0 if absolutely
 *          nothing fits
 * @param   display     pointer to the X11 display handle
 * @param   min_keycode the lowest value in use as a keycode on this server
 * @param   max_keycode the highest value in use as a keycode on this server
 */
static int
X11DRV_KEYBOARD_DetectLayout (Display *display, int min_keycode,
                              int max_keycode)
{
  /** Counter variable for iterating through the keyboard layout tables. */
  unsigned current;
  /** The best candidate so far for the layout. */
  unsigned kbd_layout = 0;
  /** The number of matching keys in the current best candidate layout. */
  unsigned max_score = 0;
  /** The number of changes of scan-code direction in the current
      best candidate. */
  unsigned max_seq = 0;
  /** Table for the current keycode to keysym mapping. */
  char ckey[256][2];
  /** Counter variable representing a keycode */
  unsigned keyc;

  /* Fill in our keycode to keysym mapping table. */
  memset( ckey, 0, sizeof(ckey) );
  for (keyc = min_keycode; keyc <= max_keycode; keyc++) {
      /* get data for keycodes from X server */
      KeySym keysym = XKeycodeToKeysym (display, keyc, 0);
      /* We leave keycodes which will definitely not be in the lookup tables
         marked with 0 so that we know that we know not to look them up when
         we scan the tables. */
      if (   (0xFF != (keysym >> 8))     /* Non-character key */
          && (0x1008FF != (keysym >> 8)) /* XFree86 vendor keys */
          && (0x20 != keysym)            /* Spacebar */
          && (0xFE03 != keysym)          /* ISO level3 shift, aka AltGr */
         ) {
          ckey[keyc][0] = keysym & 0xFF;
          ckey[keyc][1] = XKeycodeToKeysym(display, keyc, 1) & 0xFF;
      }
  }

  /* Now scan the lookup tables, looking for one that is as close as
     possible to our current keycode to keysym mapping. */
  for (current = 0; main_key_tab[current].comment; current++) {
    /** How many keys have matched so far in this layout? */
    unsigned match = 0;
    /** How many keys have not changed the direction? */
    unsigned seq = 0;
    /** Pointer to the layout we are currently comparing against. */
    const char (*lkey)[MAIN_LEN][2] = main_key_tab[current].key;
    /** For detecting dvorak layouts - in which direction do the server's
        keycodes seem to be running?  We count the number of times that
        this direction changes as an additional hint as to how likely this
        layout is to be the right one. */
    int direction = 1;
    /** The keycode of the last key that we matched.  This is used to
        determine the direction that the keycodes are running in. */
    int pkey = -1;
    TRACE("Attempting to match against \"%s\"\n", main_key_tab[current].comment);
    for (keyc = min_keycode; keyc <= max_keycode; keyc++) {
      if (0 != ckey[keyc][0]) {
        /** The candidate key in the current layout for this keycode. */
        int key;
        /** Does this key match? */
        int ok = 0;
	/* search for a match in layout table */
	for (key = 0; (key < MAIN_LEN) && (0 == ok); key++) {
	  if (   ((*lkey)[key][0] == ckey[keyc][0])
	      && ((*lkey)[key][1] == ckey[keyc][1])
             ) {
              ok = 1;
	  }
	}
	/* count the matches and mismatches */
	if (0 != ok) {
	  match++;
          /* How well in sequence are the keys?  For dvorak layouts. */
	  if (key > pkey) {
	    if (1 == direction) {
	      ++seq;
            } else {
	      direction = -1;
            }
	  }
	  if (key < pkey) {
	    if (1 != direction) {
	      ++seq;
            } else {
	      direction = 1;
            }
	  }
	  pkey = key;
	} else {
#ifdef DEBUG
          /* print spaces instead of \0's */
          char str[3] = "  ";
          if ((ckey[keyc][0] > 32) && (ckey[keyc][0] < 127)) {
              str[0] = ckey[keyc][0];
          }
          if ((ckey[keyc][0] > 32) && (ckey[keyc][0] < 127)) {
              str[0] = ckey[keyc][0];
          }
          TRACE_(key)("mismatch for keycode %d, keysym \"%s\" (0x%.2hx 0x%.2hx)\n",
                       keyc, str, ckey[keyc][0], ckey[keyc][1]);
#endif /* DEBUG defined */
	}
      }
    }
    TRACE("matches=%d, seq=%d\n",
	   match, seq);
    if (   (match > max_score)
	|| ((match == max_score) && (seq > max_seq))
       ) {
      /* best match so far */
      kbd_layout = current;
      max_score = match;
      max_seq = seq;
    }
  }
  /* we're done, report results if necessary */
  TRACE("detected layout is \"%s\", matches=%d, seq=%d\n",
        main_key_tab[kbd_layout].comment, max_score, max_seq);
  return kbd_layout;
}

/**
 * Initialise the X11 keyboard driver.  In practice, this means building
 * up an internal table to map X11 keycodes to their equivalent PC scan
 * codes.
 *
 * @warning not re-entrant
 * @returns 1 if the layout found was optimal, 0 if it was not.  This is
 *          for diagnostic purposes
 * @param   display a pointer to the X11 display
 */
int X11DRV_InitKeyboard(Display *display)
{
    KeySym keysym;
    unsigned scan;
    int keyc, keyn;
    const char (*lkey)[MAIN_LEN][2];
    int min_keycode, max_keycode;
    int kbd_layout;
    unsigned matches = 0, entries = 0;

    XDisplayKeycodes(display, &min_keycode, &max_keycode);

    /* Detect the keyboard layout */
    kbd_layout = X11DRV_KEYBOARD_DetectLayout(display, min_keycode,
                                              max_keycode);
    lkey = main_key_tab[kbd_layout].key;

    /* Now build a conversion array :
     * keycode -> scancode + extended */

    for (keyc = min_keycode; keyc <= max_keycode; keyc++)
    {
        keysym = XKeycodeToKeysym(display, keyc, 0);
        scan = 0;
        if (keysym)  /* otherwise, keycode not used */
        {
            if ((keysym >> 8) == 0xFF)         /* non-character key */
            {
                scan = nonchar_key_scan[keysym & 0xff];
		/* set extended bit when necessary */
            } else if ((keysym >> 8) == 0x1008FF) { /* XFree86 vendor keys */
/* innotek FIX - multimedia/internet keys */
                scan = xfree86_vendor_key_scan[keysym & 0xff];
            } else if (keysym == 0x20) {                 /* Spacebar */
		scan = 0x39;
/* innotek FIX - AltGr support */
            } else if (keysym == 0xFE03) {               /* ISO level3 shift, aka AltGr */
		scan = 0x138;
	    } else {
	      unsigned found = 0;

	      /* we seem to need to search the layout-dependent scancodes */
              char unshifted = keysym & 0xFF;
              char shifted = XKeycodeToKeysym(display, keyc, 1) & 0xFF;
	      /* find a key which matches */
	      for (keyn = 0; (0 == found) && (keyn<MAIN_LEN); keyn++) {
                if (   ((*lkey)[keyn][0] == unshifted)
                    && ((*lkey)[keyn][1] == shifted)
                   ) {
                   found = 1;
                }
	      }
	      if (0 != found) {
		/* got it */
		scan = main_key_scan[keyn - 1];
		++matches;
	      }
#ifdef DEBUG
              if (0 == scan) {
                /* print spaces instead of \0's */
                char str[3] = "  ";
                if ((unshifted > 32) && (unshifted < 127)) {
                    str[0] = unshifted;
                }
                if ((shifted > 32) && (shifted < 127)) {
                    str[1] = shifted;
                }
                TRACE_(key)("No match found for keycode %d, keysym \"%s\" (0x%x 0x%x)\n",
                             keyc, str, unshifted, shifted);
            }
#endif /* DEBUG defined */
	    }
        }
        keyc2scan[keyc] = scan;
    } /* for */
    /* Did we find a match for all keys in the layout?  Count them first. */
    for (entries = 0, keyn = 0; keyn < MAIN_LEN; ++keyn) {
        if (   (0 != (*lkey)[keyn][0])
            && (0 != (*lkey)[keyn][1])
           ) {
            ++entries;
        }
    }
    TRACE("Finished mapping keyboard, matches=%d, entries=%d\n", matches, entries);
    if (matches != entries)
    {
        return 0;
    }
    return 1;
}
