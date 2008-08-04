/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * X11 keyboard handler library
 */

/* This code is originally from the Wine project. */

/*
 * X11 keyboard driver
 *
 * Copyright 1993 Bob Amstadt
 * Copyright 1996 Albrecht Kleine
 * Copyright 1997 David Faure
 * Copyright 1998 Morten Welinder
 * Copyright 1998 Ulrich Weigand
 * Copyright 1999 Ove K�ven
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
#include <stdlib.h>
#include <stdio.h>

/** 
 * Array containing the current mapping of keycodes to scan codes, detected
 * using the keyboard layout algorithm in X11DRV_InitKeyboardByLayout.
 */
static unsigned keyc2scan[256];
/** 
 * Whether a keyboard was detected with a well-known keycode to scan code
 * mapping.
 */
static unsigned use_builtin_table = 0;
/** The index of the well-known keycode to scan code mapping in our table. */
static unsigned builtin_table_number;
/** Whether to output basic debugging information to standard output */
static int log_kb_1 = 0;
/** Whether to output verbose debugging information to standard output */
static int log_kb_2 = 0;

/** Output basic debugging information if wished */
#define LOG_KB_1(a) \
do { \
    if (log_kb_1) { \
        printf a; \
    } \
} while (0)

/** Output verbose debugging information if wished */
#define LOG_KB_2(a) \
do { \
    if (log_kb_2) { \
        printf a; \
    } \
} while (0)

/** Keyboard layout tables for guessing the current keyboard layout. */
#include "keyboard-tables.h"

/** Tables of keycode to scan code mappings for well-known keyboard types. */
#include "keyboard-types.h"

/**
  * Translate a keycode in a key event to a scan code, using the lookup table
  * which we constructed earlier or a well-known mapping from our mapping
  * table.
  *
  * @returns the scan code number, with 0x100 added for extended scan codes
  * @param code the X11 key code to be looked up
  */

unsigned X11DRV_KeyEvent(KeyCode code)
{
    unsigned key;
    if (use_builtin_table != 0)
        key = main_keyboard_type_scans[builtin_table_number][code];
    else
        key = keyc2scan[code];
    return key;
}

/**
 * Called from X11DRV_InitKeyboardByLayout
 *  See the comments for that function for a description what this function
 *  does.
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
          && (0x1005FF != (keysym >> 8)) /* Sun keys */
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
    LOG_KB_2(("Attempting to match against \"%s\"\n", main_key_tab[current].comment));
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
          LOG_KB_2(("Mismatch for keycode %d, keysym \"%s\" (0x%.2hx 0x%.2hx)\n",
                       keyc, str, ckey[keyc][0], ckey[keyc][1]));
#endif /* DEBUG defined */
	}
      }
    }
    LOG_KB_2(("Matches=%d, seq=%d\n", match, seq));
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
  LOG_KB_1(("Detected layout is \"%s\", matches=%d, seq=%d\n",
        main_key_tab[kbd_layout].comment, max_score, max_seq));
  return kbd_layout;
}

/**
 * Initialise the X11 keyboard driver by building up a table to convert X11
 * keycodes to scan codes using a heuristic based on comparing the current
 * keyboard map to known international keyboard layouts.
 * The basic idea is to examine each key in the current layout to see which
 * characters it produces in its normal and its "shifted" state, and to look
 * for known keyboard layouts which it could belong to.  We then guess the
 * current layout based on the number of matches we find.
 * One difficulty with this approach is so-called Dvorak layouts, which are
 * identical to non-Dvorak layouts, but with the keys in a different order.
 * To deal with this, we compare the different candidate layouts to see in
 * which one the X11 keycodes would be most sequential and hope that they
 * really are layed out more or less sequentially.
 * 
 * The actual detection of the current layout is done in the sub-function
 * X11DRV_KEYBOARD_DetectLayout.  Once we have determined the layout, since we 
 * know which PC scan code corresponds to each key in the layout, we can use
 * this information to associate the scan code with an X11 keycode, which is
 * what the rest of this function does.
 *
 * @warning not re-entrant
 * @returns 1 if the layout found was optimal, 0 if it was not.  This is
 *          for diagnostic purposes
 * @param   display a pointer to the X11 display
 */
static unsigned
X11DRV_InitKeyboardByLayout(Display *display)
{
    KeySym keysym;
    unsigned scan;
    int keyc, keyn;
    const char (*lkey)[MAIN_LEN][2];
    int min_keycode, max_keycode;
    int kbd_layout;
    unsigned matches = 0, entries = 0;

    /* Should we log to standard output? */
    if (NULL != getenv("LOG_KB_PRIMARY")) {
        log_kb_1 = 1;
    }
    if (NULL != getenv("LOG_KB_SECONDARY")) {
        log_kb_1 = 1;
        log_kb_2 = 1;
    }
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
/* VirtualBox FIX - multimedia/internet keys */
                scan = xfree86_vendor_key_scan[keysym & 0xff];
            } else if ((keysym >> 8) == 0x1005FF) { /* Sun keys */
                scan = sun_key_scan[keysym & 0xff];
            } else if (keysym == 0x20) {                 /* Spacebar */
		scan = 0x39;
/* VirtualBox FIX - AltGr support */
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
              if (0 == scan) {
                /* print spaces instead of \0's */
                char str[3] = "  ";
                if ((unshifted > 32) && (unshifted < 127)) {
                    str[0] = unshifted;
                }
                if ((shifted > 32) && (shifted < 127)) {
                    str[1] = shifted;
                }
                LOG_KB_1(("No match found for keycode %d, keysym \"%s\" (0x%x 0x%x)\n",
                             keyc, str, unshifted, shifted));
              } else if ((keyc > 8) && (keyc < 97) && (keyc - scan != 8)) {
                /* print spaces instead of \0's */
                char str[3] = "  ";
                if ((unshifted > 32) && (unshifted < 127)) {
                    str[0] = unshifted;
                }
                if ((shifted > 32) && (shifted < 127)) {
                    str[1] = shifted;
                }
                LOG_KB_1(("Warning - keycode %d, keysym \"%s\" (0x%x 0x%x) was matched to scancode %d\n",
                             keyc, str, unshifted, shifted, scan));
              }
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
    LOG_KB_1(("Finished mapping keyboard, matches=%d, entries=%d\n", matches, entries));
    if (matches != entries)
    {
        return 0;
    }
    return 1;
}

static unsigned
X11DRV_InitKeyboardByType(Display *display)
{
    unsigned i = 0, found = 0;
    
    for (; (main_keyboard_type_list[i].comment != NULL) && (0 == found); ++i)
        if (   (XKeysymToKeycode(display, XK_Control_L) == main_keyboard_type_list[i].lctrl)
            && (XKeysymToKeycode(display, XK_Shift_L)   == main_keyboard_type_list[i].lshift)
            && (XKeysymToKeycode(display, XK_Caps_Lock) == main_keyboard_type_list[i].capslock)
            && (XKeysymToKeycode(display, XK_Tab)       == main_keyboard_type_list[i].tab)
            && (XKeysymToKeycode(display, XK_Escape)    == main_keyboard_type_list[i].esc)
            && (XKeysymToKeycode(display, XK_Return)    == main_keyboard_type_list[i].enter)
            && (XKeysymToKeycode(display, XK_Up)        == main_keyboard_type_list[i].up)
            && (XKeysymToKeycode(display, XK_Down)      == main_keyboard_type_list[i].down)
            && (XKeysymToKeycode(display, XK_Left)      == main_keyboard_type_list[i].left)
            && (XKeysymToKeycode(display, XK_Right)     == main_keyboard_type_list[i].right)
            && (XKeysymToKeycode(display, XK_F1)        == main_keyboard_type_list[i].f1)
            && (XKeysymToKeycode(display, XK_F2)        == main_keyboard_type_list[i].f2)
            && (XKeysymToKeycode(display, XK_F3)        == main_keyboard_type_list[i].f3)
            && (XKeysymToKeycode(display, XK_F4)        == main_keyboard_type_list[i].f4)
            && (XKeysymToKeycode(display, XK_F5)        == main_keyboard_type_list[i].f5)
            && (XKeysymToKeycode(display, XK_F6)        == main_keyboard_type_list[i].f6)
            && (XKeysymToKeycode(display, XK_F7)        == main_keyboard_type_list[i].f7)
            && (XKeysymToKeycode(display, XK_F8)        == main_keyboard_type_list[i].f8)
           )
            found = 1;
    use_builtin_table = found;
    if (found != 0)
        builtin_table_number = i - 1;
    return found;
}

/**
 * Initialise the X11 keyboard driver by finding which X11 keycodes correspond
 * to which PC scan codes.  If the keyboard being used is not a PC keyboard,
 * the X11 keycodes will be mapped to the scan codes which the equivalent keys
 * on a PC keyboard would use.
 * 
 * We use two algorithms to try to determine the mapping.  See the comments
 * attached to the two algorithm functions (X11DRV_InitKeyboardByLayout and
 * X11DRV_InitKeyboardByType) for descriptions of the algorithms used.  Both
 * functions tell us on return whether they think that they have correctly
 * determined the mapping.  If both functions claim to have determined the
 * mapping correctly, we prefer the second (ByType).  However, if neither does
 * then we prefer the first (ByLayout), as it produces a fuzzy result which is
 * still likely to be partially correct.
 *
 * @warning not re-entrant
 * @returns 1 if the layout found was optimal, 0 if it was not.  This is
 *          for diagnostic purposes
 * @param   display     a pointer to the X11 display
 * @param   byLayoutOK  diagnostic - set to one if detection by layout
 *                      succeeded, and to 0 otherwise
 * @param   byTypeOK    diagnostic - set to one if detection by type
 *                      succeeded, and to 0 otherwise
 */
unsigned X11DRV_InitKeyboard(Display *display, unsigned *byLayoutOK, unsigned *byTypeOK)
{
    unsigned byLayout = X11DRV_InitKeyboardByLayout(display);
    unsigned byType   = X11DRV_InitKeyboardByType(display);
    *byLayoutOK = byLayout;
    *byTypeOK   = byType;
    return (byLayout || byType) ? 1 : 0;
}
