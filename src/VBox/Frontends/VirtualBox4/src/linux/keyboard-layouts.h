/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * X11 keyboard driver translation tables (keyboard layouts)
 *
 */

/* This code is originally from the Wine project. */

/*
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

#ifndef ___VBox_keyboard_tables_h
# error This file must be included from within keyboard-layouts.h
#endif /* ___VBox_keyboard_tables_h */

/* This file contains a more or less complete dump of all keyboard
   layouts known to my version of X.org.  Duplicate layouts have
   been removed to save space and lookup time, and the Japanese
   layout has been manually corrected, due to differences in handling
   between 105 and 106-key keyboards.

   Note that contrary to the original tables in the Wine source code,
   these tables simply contain the X keysym values truncated to the
   least significant byte.  In fact, there is no need to do any
   additional translation of the values (the original code translated
   them to whatever character set was deemed appropriate, rather 
   inconsistently) as long as we use the same algorithm for creating
   the tables and doing the lookups.

   The last three entries in the tables are respectively the 102nd
   key on 102/105/106 key keyboards, the extra key on Brazilian and
   Japanese keyboards and the Yen key on Japanese keyboards.
   The layout-switching keys on Japanese and Korean keyboards are
   dealt with elsewhere. */

/* U.S. English */
static const char main_key_us[MAIN_LEN][2] =
{
"`~","1!","2@","3#","4$","5%","6^","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","[{","]}",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL",";:","'\"","\\|",
"zZ","xX","cC","vV","bB","nN","mM",",<",".>","/?","<>","\x0\x0","\x0\x0"
};

/* U.S. English, International (with dead keys) */
static const char main_key_us_intl[MAIN_LEN][2] =
{
"PS","1!","2@","3#","4$","5%","6R","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","[{","]}",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL",";:","QW","\\|",
"zZ","xX","cC","vV","bB","nN","mM",",<",".>","/?","<>","\x0\x0","\x0\x0"
};

/* U.S. English, Dvorak */
static const char main_key_us_dvorak[MAIN_LEN][2] =
{
"`~","1!","2@","3#","4$","5%","6^","7&","8*","9(","0)","[{","]}",
"'\"",",<",".>","pP","yY","fF","gG","cC","rR","lL","/?","=+",
"aA","oO","eE","uU","iI","dD","hH","tT","nN","sS","-_","\\|",
";:","qQ","jJ","kK","xX","bB","mM","wW","vV","zZ","<>","\x0\x0","\x0\x0"
};

/* U.S. English, Left handed Dvorak */
static const char main_key_us_dvorak_l[MAIN_LEN][2] =
{
"`~","[{","]}","/?","pP","fF","mM","lL","jJ","4$","3#","2@","1!",
";:","qQ","bB","yY","uU","rR","sS","oO",".>","6^","5%","=+",
"-_","kK","cC","dD","tT","hH","eE","aA","zZ","8*","7&","\\|",
"'\"","xX","gG","vV","wW","nN","iI",",<","0)","9(","<>","\x0\x0","\x0\x0"
};

/* U.S. English, Right handed Dvorak */
static const char main_key_us_dvorak_r[MAIN_LEN][2] =
{
"`~","1!","2@","3#","4$","jJ","lL","mM","fF","pP","/?","[{","]}",
"5%","6^","qQ",".>","oO","rR","sS","uU","yY","bB",";:","=+",
"7&","8*","zZ","aA","eE","hH","tT","dD","cC","kK","-_","\\|",
"9(","0)","xX",",<","iI","nN","wW","vV","gG","'\"","<>","\x0\x0","\x0\x0"
};

/* U.S. English, Classic Dvorak */
static const char main_key_us_dvorak_classic[MAIN_LEN][2] =
{
"`~","[{","7&","5%","3#","1!","9(","0)","2@","4$","6^","8*","]}",
"/?",",<",".>","pP","yY","fF","gG","cC","rR","lL","'\"","=+",
"aA","oO","eE","uU","iI","dD","hH","tT","nN","sS","-_","\\|",
";:","qQ","jJ","kK","xX","bB","mM","wW","vV","zZ","<>","\x0\x0","\x0\x0"
};

/* U.S. English, Russian phonetic */
static const char main_key_us_rus[MAIN_LEN][2] =
{
"\xa3\xb3","1!","2@","3#","4$","5%","6^","7&","8*","9(","0)","-_","\xdf\xff",
"\xd1\xf1","\xd7\xf7","\xc5\xe5","\xd2\xf2","\xd4\xf4","\xd9\xf9","\xd5\xf5","\xc9\xe9","\xcf\xef","\xd0\xf0","\xdb\xfb","\xdd\xfd",
"\xc1\xe1","\xd3\xf3","\xc4\xe4","\xc6\xe6","\xc7\xe7","\xc8\xe8","\xca\xea","\xcb\xeb","\xcc\xec","\xde\xfe","\xc0\xe0","\xdc\xfc",
"\xda\xfa","\xd8\xf8","\xc3\xe3","\xd6\xf6","\xc2\xe2","\xce\xee","\xcd\xed",",<",".>","/?","<>","\x0\x0","\x0\x0"
};

/* Afghanistan */
static const char main_key_af[MAIN_LEN][2] =
{
"\xd\xf7","\xf1!","\xf2l","\xf3k","\xf4\xb","\xf5j","\xf6\xd7","\xf7\xac","\xf8*","\xf9)","\xf0(","-\xe0","+=",
"\xd6\xf2","\xd5\xec","\xcb\xed","\xe2\xeb","\xe1\xef","\xda\xf0","\xd9\xee","\xe7\xf1","\xce]","\xcd[","\xcc}","\x86{",
"\xd4\xc4","\xd3\xc6","\xcc\xea","\xc8\xc5","\xe4\xc3","\xc7\xc2","\xca\xc9","\xe6\xbb","\xe5\xab","\xa9:","\xaf\xbb","\\|",
"\xd8\xe3","\xd7S","\xd2\x98","\xd1p","\xd0\xc","\xcfT","~\xc1","\xe8>",".<","/\xbf","<>","\x0\x0","\x0\x0"
};

/* Afghanistan, Pashto */
static const char main_key_af_ps[MAIN_LEN][2] =
{
"\xd\xf7","\xf1!","\xf2l","\xf3k","\xf4\xb","\xf5j","\xf6\xd7","\xf7\xbb","\xf8\xab","\xf9)","\xf0(","-\xe0","+=",
"\xd6\xf2","\xd5\xec","\xcb\xed","\xe2\xeb","\xe1\xef","\xda\xf0","\xd9\xee","\xe7\xf1","\xce\x81","\xcd\x85","\xcc]","\x86[",
"\xd4\x9a","\xd3\xc6","\xcc\xea","\xc8~","\xe4\xc3","\xc7\xc2","\xca|","\xe6\xbc","\xe5)","\xa9:","\xab\xbb","\\*",
"\xcd""8","\xd0""7","\xd2\x98","\xd1!","\xd0\xc","\xcf\x89","\x93$","\xe8\xc","\x96.","/\xbf","<>","\x0\x0","\x0\x0"
};

/* Afghanistan, Southern Uzbek */
static const char main_key_af_uz[MAIN_LEN][2] =
{
"\xd\xf7","\xf1!","\xf2l","\xf3k","\xf4\xb","\xf5j","\xf6\xd7","\xf7\xac","\xf8*","\xf9)","\xf0(","-\xe0","+=",
"\xd6\xf2","\xd5\xec","\xcb\xed","\xe2\xeb","\xe1\xef","\xda\xf0","\xd9\xee","\xe7\xf1","\xce]","\xcd[","\xcc}","\x86{",
"\xd4\xc4","\xd3\xc6","\xcc\xea","\xc8\xd0","\xe4\xc3","\xc7\xc2","\xca\xc9","\xe6\xbb","\xe5\xab","\xa9:","\xaf\xbb","\\|",
"\xd8\xc9","\xd7\xc7","\xd2\x98","\xd1%","\xd0\xc","\xcfT","~\xc1","\xe8>",".<","/\xbf","<>","\x0\x0","\x0\x0"
};

/* Arabic */
static const char main_key_ara[MAIN_LEN][2] =
{
"\xd0\xf1","1!","2@","3#","4$","5%","6^","7&","8*","9(","0)","-_","=+",
"\xd6\xee","\xd5\xeb","\xcb\xef","\xe2\xec","\xe1\xf9","\xda\xc5","\xd9`","\xe7\xf7","\xce\xd7","\xcd\xbb","\xcc{","\xcf}",
"\xd4\\","\xd3S","\xea[","\xc8]","\xe4\xf7","\xc7\xc3","\xca\xe0","\xe6\xac","\xe5/","\xe3:","\xd7\"","<>",
"\xc6~","\xc1\xf2","\xc4\xf0","\xd1\xed","\xfb\xf5","\xe9\xc2","\xc9'","\xe8,","\xd2.","\xd8\xbf","|\xa6","\x0\x0","\x0\x0"
};

/* Arabic, azerty */
static const char main_key_ara_azerty[MAIN_LEN][2] =
{
"\xd0\xf1","&1","\xe9""2","\"3","'4","(5","-6","\xe8""7","_8","\xe7""9","\xe0""0",")\xb0","=+",
"\xd6\xee","\xd5\xeb","\xcb\xef","\xe2\xec","\xe1\xf9","\xda\xc5","\xd9`","\xe7\xf7","\xce\xd7","\xcd\xbb","\xcc{","\xcf}",
"\xd4\\","\xd3S","\xea[","\xc8]","\xe4\xf7","\xc7\xc3","\xca\xe0","\xe6\xac","\xe5/","\xe3:","\xd7\"","<>",
"\xc6~","\xc1\xf2","\xc4\xf0","\xd1\xed","\xfb\xf5","\xe9\xc2","\xc9'","\xe8,","\xd2.","\xd8\xbf","|\xa6","\x0\x0","\x0\x0"
};

/* Arabic, azerty/digits */
static const char main_key_ara_azerty_digits[MAIN_LEN][2] =
{
"\xd0\xf1","&a","\xe9""b","\"c","'d","(e","-f","\xe8g","_h","\xe7i","\xe0`",")\xb0","=+",
"\xd6\xee","\xd5\xeb","\xcb\xef","\xe2\xec","\xe1\xf9","\xda\xc5","\xd9`","\xe7\xf7","\xce\xd7","\xcd\xbb","\xcc{","\xcf}",
"\xd4\\","\xd3S","\xea[","\xc8]","\xe4\xf7","\xc7\xc3","\xca\xe0","\xe6\xac","\xe5/","\xe3:","\xd7\"","<>",
"\xc6~","\xc1\xf2","\xc4\xf0","\xd1\xed","\xfb\xf5","\xe9\xc2","\xc9'","\xe8,","\xd2.","\xd8\xbf","|\xa6","\x0\x0","\x0\x0"
};

/* Arabic, digits */
static const char main_key_ara_digits[MAIN_LEN][2] =
{
"\xd0\xf1","a!","b@","c#","d$","e%","f^","g&","h*","i(","`)","-_","=+",
"\xd6\xee","\xd5\xeb","\xcb\xef","\xe2\xec","\xe1\xf9","\xda\xc5","\xd9`","\xe7\xf7","\xce\xd7","\xcd\xbb","\xcc{","\xcf}",
"\xd4\\","\xd3S","\xea[","\xc8]","\xe4\xf7","\xc7\xc3","\xca\xe0","\xe6\xac","\xe5/","\xe3:","\xd7\"","<>",
"\xc6~","\xc1\xf2","\xc4\xf0","\xd1\xed","\xfb\xf5","\xe9\xc2","\xc9'","\xe8,","\xd2.","\xd8\xbf","|\xa6","\x0\x0","\x0\x0"
};

/* Arabic, Buckwalter */
static const char main_key_ara_buckwalter[MAIN_LEN][2] =
{
"p\xf1","a\xff","b\xff","c\xff","d\xd4","ej","f\xff","g\xc4","h\xd0","i>","`?","-\xe0","=+",
"\xe2\xff","\xe8\xc4","\xff\xd9","\xd1\xff","\xca\xd7","\xea\xe9","\xef\xff","\xf0\xc5","\xf2\xc3","\xc9\xff","\xffq","\xff\xc6",
"\xee\xc7","\xd3\xd5","\xcf\xd6","\xe1\xeb","\xda\xff","\xe7\xcd","\xcc\xff","\xe3\xed","\xe4\xff","\xbb\xff","\xc1\xff","\xff\xc2",
"\xd2\xd8","\xce\xff","\xff\xff","\xcb\xff","\xc8\xff","\xe6\xec","\xe5\xff","\xac\xc5","\xd4\xc3","\xff\xbf","<>","\x0\x0","\x0\x0"
};

/* Albania */
static const char main_key_al[MAIN_LEN][2] =
{
"\\|","1!","2\"","3#","4$","5%","6^","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","zZ","uU","iI","oO","pP","\xe7\xc7","@'",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xeb\xcb","[{","]}",
"yY","xX","cC","vV","bB","nN","mM",",;",".:","/?","<>","\x0\x0","\x0\x0"
};

/* Armenia */
static const char main_key_am[MAIN_LEN][2] =
{
"]\\","\x86V","qA","\x13\x14",",$","\x89&","^%","$\x87","[\xbc",")(","\x85U","g7","rB",
"sC","\x83S","b2","}M","tD","xH","\x82R","o?","h8","i9","n>","\x81Q",
"{K","~N","c3","e5","a1","vF","k;","\x7fO","p@","zJ","\x80P","\xbb\xab",
"j:","d4","yI","uE","f6","l<","\x84T","m=","wG","|L","?\x8a","\x0\x0","\x0\x0"
};

/* Armenia, Phonetic */
static const char main_key_am_phonetic[MAIN_LEN][2] =
{
"]\\","g7","i9","\x83S","qA","{K","\x82R","\x87\x87","\x80P","yI","sC","-\x15","j:",
"\x84T","xH","e5","|L","\x7fO","h8","\x82R","k;","\x85U","zJ","m=","n>",
"a1","}M","d4","\x86V","c3","p@","uE","o?","l<",";\x89","[\"","wG",
"f6","rB","\x81Q","~N","b2","vF","tD",",\xab","$\xbb","/^","?\x8a","\x0\x0","\x0\x0"
};

/* Armenia, Eastern */
static const char main_key_am_eastern[MAIN_LEN][2] =
{
"]\\","\x89\xb1","qA","uE","[\xb3",",\xb4","-\xb9",".\x87","\xab(","\xbb)","\x85U","|L","j:",
"m=","\x82R","g7","\x80P","\x7fO","e5","h8","k;","xH","zJ","yI","{K",
"a1","}M","d4","\x86V","c3","p@","sC","o?","l<","i9","\x83S","'^",
"f6","\x81Q","\x84T","~N","b2","vF","tD","wG","rB","n>","<>","\x0\x0","\x0\x0"
};

/* Armenia, Western */
static const char main_key_am_western[MAIN_LEN][2] =
{
"]\\","\x89\xb1","qA","uE","[\xb3",",\xb4","-\xb9",".\x87","\xab(","\xbb)","\x85U","|L","j:",
"m=","~N","g7","\x80P","d4","e5","h8","k;","xH","b2","yI","{K",
"a1","}M","\x7fO","\x86V","o?","p@","sC","\x84T","l<","i9","\x83S","'^",
"f6","\x81Q","c3","\x82R","zJ","vF","tD","wG","rB","n>","<>","\x0\x0","\x0\x0"
};

/* Armenia, Alternative Eastern */
static const char main_key_am_eastern_alt[MAIN_LEN][2] =
{
"]\\","\x89\xb1","qA","uE","[\xb3",",\xb4","-\xb9",".\x87","\xab(","\xbb)","\x85U","|L","j:",
"m=","\x82R","g7","\x80P","\x7fO","e5","h8","k;","xH","zJ","yI","{K",
"a1","}M","d4","\x86V","\x84T","p@","sC","o?","l<","i9","\x83S","'^",
"f6","\x81Q","c3","~N","b2","vF","tD","wG","rB","n>","<>","\x0\x0","\x0\x0"
};

/* Azerbaijan */
static const char main_key_az[MAIN_LEN][2] =
{
"`~","1!","2@","3#","4$","5%","6:","7?","8*","9(","0)","-_","=+",
"qQ","\xfc\xdc","eE","rR","tT","yY","uU","i\xa9","oO","pP","\xf6\xd6","\xbb\xab",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xb9I","Y\x8f","\\|",
"zZ","xX","cC","vV","bB","nN","mM","\xe7\xc7","\xba\xaa",".,","<>","\x0\x0","\x0\x0"
};

/* Azerbaijan, Cyrillic */
static const char main_key_az_cyrillic[MAIN_LEN][2] =
{
"\x0\x0","\x0\x0","2\"","3#","4;","\x0\x0","6:","7?","8*","9(","0)","-_","\x0\x0",
"\xa8\xb8","\xaf\xae","\xd5\xf5","\xcb\xeb","\xc5\xe5","\xce\xee","\xc7\xe7","\xdb\xfb","\xbb\xba","\xda\xfa","\xc8\xe8","\xb9\xb8",
"\xc6\xe6","\xd9\xf9","\xd7\xf7","\xc1\xe1","\xd0\xf0","\xd2\xf2","\xcf\xef","\xcc\xec","\xc4\xe4","\xd6\xf6","\x9d\x9c","\\|",
"\xd9\xd8","\xde\xfe","\xd3\xf3","\xcd\xed","\xc9\xe9","\xd4\xf4","\x93\x92","\xc2\xe2","\xe9\xe8",".,","<>","\x0\x0","\x0\x0"
};

/* Belarus */
static const char main_key_by[MAIN_LEN][2] =
{
"\xa3\xb3","\x0\x0","2\"","\x0\x0","4;","5%","6:","7?","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0",
"\xca\xea","\xc3\xe3","\xd5\xf5","\xcb\xeb","\xc5\xe5","\xce\xee","\xc7\xe7","\xdb\xfb","\xae\xbe","\xda\xfa","\xc8\xe8","''",
"\xc6\xe6","\xd9\xf9","\xd7\xf7","\xc1\xe1","\xd0\xf0","\xd2\xf2","\xcf\xef","\xcc\xec","\xc4\xe4","\xd6\xf6","\xdc\xfc","/|",
"\xd1\xf1","\xde\xfe","\xd3\xf3","\xcd\xed","\xa6\xb6","\xd4\xf4","\xd8\xf8","\xc2\xe2","\xc0\xe0",".,","|\xa6","\x0\x0","\x0\x0"
};

/* Belarus, Winkeys */
static const char main_key_by_winkeys[MAIN_LEN][2] =
{
"\xa3\xb3","\x0\x0","2\"","3#","4;","5%","6:","7?","8*","\x0\x0","\x0\x0","\x0\x0","\x0\x0",
"\xca\xea","\xc3\xe3","\xd5\xf5","\xcb\xeb","\xc5\xe5","\xce\xee","\xc7\xe7","\xdb\xfb","\xae\xbe","\xda\xfa","\xc8\xe8","''",
"\xc6\xe6","\xd9\xf9","\xd7\xf7","\xc1\xe1","\xd0\xf0","\xd2\xf2","\xcf\xef","\xcc\xec","\xc4\xe4","\xd6\xf6","\xdc\xfc","/|",
"\xd1\xf1","\xde\xfe","\xd3\xf3","\xcd\xed","\xa6\xb6","\xd4\xf4","\xd8\xf8","\xc2\xe2","\xc0\xe0",".,","|\xa6","\x0\x0","\x0\x0"
};

/* Belgium */
static const char main_key_be[MAIN_LEN][2] =
{
"\xb2\xb3","&1","\xe9""2","\"3","'4","(5","\xa7""6","\xe8""7","!8","\xe7""9","\xe0""0",")\xb0","-_",
"aA","zZ","eE","rR","tT","yY","uU","iI","oO","pP","RW","$*",
"qQ","sS","dD","fF","gG","hH","jJ","kK","lL","mM","\xf9%","\xb5\xa3",
"wW","xX","cC","vV","bB","nN",",?",";.",":/","=+","<>","\x0\x0","\x0\x0"
};

/* Belgium, Eliminate dead keys */
static const char main_key_be_nodeadkeys[MAIN_LEN][2] =
{
"\xb2\xb3","&1","\xe9""2","\"3","'4","(5","\xa7""6","\xe8""7","!8","\xe7""9","\xe0""0",")\xb0","-_",
"aA","zZ","eE","rR","tT","yY","uU","iI","oO","pP","^\xa8","$*",
"qQ","sS","dD","fF","gG","hH","jJ","kK","lL","mM","\xf9%","\xb5\xa3",
"wW","xX","cC","vV","bB","nN",",?",";.",":/","=+","<>","\x0\x0","\x0\x0"
};

/* Belgium, Sun dead keys */
static const char main_key_be_sundeadkeys[MAIN_LEN][2] =
{
"\xb2\xb3","&1","\xe9""2","\"3","'4","(5","\xa7""6","\xe8""7","!8","\xe7""9","\xe0""0",")\xb0","-_",
"aA","zZ","eE","rR","tT","yY","uU","iI","oO","pP","\x1\x4","$*",
"qQ","sS","dD","fF","gG","hH","jJ","kK","lL","mM","\xf9%","\xb5\xa3",
"wW","xX","cC","vV","bB","nN",",?",";.",":/","=+","<>","\x0\x0","\x0\x0"
};

/* Bangladesh */
static const char main_key_bd[MAIN_LEN][2] =
{
"`~","\xe7!","\xe8@","\xe9#","\xea$","\xeb%","\xec^","\xed&","\xee*","\xef(","\xe6)","-_","=+",
"\x99\x82","\xaf\xdf","\xa1\xa2","\xaa\xab","\x9f\xa0","\x9a\x9b","\x9c\x9d","\xb9\x9e","\x97\x98","\xdc\xdd","[{","]}",
"\xc3\xd7","\xc1\xc2","\xbf\xc0","\xac\xad","\xcd""d","\xbe\x85","\x95\x96","\xa4\xa5","\xa6\xa7",";:","'\"","\\|",
"\x81\x83","\xcb\xcc","\xc7\xc8","\xb0\xb2","\xa8\xa3","\xb8\xb7","\xae\xb6",",<",".>","/?","<>","\x0\x0","\x0\x0"
};

/* Bangladesh, Probhat */
static const char main_key_bd_probhat[MAIN_LEN][2] =
{
"`~","\xe7!","\xe8@","\xe9#","\xea\xf3","\xeb%","\xec^","\xed\x9e","\xee\xce","\xef(","\xe6)","\xc_","=\xd",
"\xa6\xa7","\xc2\x8a","\xc0\x88","\xb0\xdc","\x9f\xa0","\x8f\x90","\xc1\x89","\xbf\x87","\x93\x94","\xaa\xab","\xc7\xc8","\xcb\xcc",
"\xbe\x85","\xb8\xb7","\xa1\xa2","\xa4\xa5","\x97\x98","\xb9\x83","\x9c\x9d","\x95\x96","\xb2\x82",";:","'\"","\\e",
"\xdf\xaf","\xb6\xdd","\x9a\x9b","\x86\x8b","\xac\xad","\xa8\xa3","\xae\x99",",\xc3","d\x81","\xcd?","<>","\x0\x0","\x0\x0"
};

/* India */
static const char main_key_in[MAIN_LEN][2] =
{
"J\x12","g\xd","hE","ii","jj","kk","ll","mm","nn","o(","f)","\x3\x3","C\xb",
"L\x14","H\x10",">\x6","@\x8","B\xa",",-","9\x19","\x17\x18","&'","\x1c\x1d","!\"","<\x1e",
"K\x13","G\xf","M\x5","?\x7","A\x9","*+","01","\x15\x16","$%","\x1a\x1b","\x1f\x20","I\x11",
"F\xe","\x2\x1",".#","()","54","23","86",",7",".d","/?","<>","\x0\x0","\x0\x0"
};

/* India, Bengali */
static const char main_key_in_ben[MAIN_LEN][2] =
{
"\x0\x0","\xe7\xe7","\xe8\xe8","\xe9\xe9","\xea\xea","\xeb\xeb","\xec\xec","\xed\xed","\xee\xee","\xef(","\xe6)","-\x83","\x8b\xc3",
"\xcc\x94","\xc8\x90","\xbe\x86","\xc0\x88","\xc2\x8a","\xac\xad","\xb9\x99","\x97\x98","\xa6\xa7","\x9c\x9d","\xa1\xa2","\xbc\x9e",
"\xcb\x93","\xc7\x8f","\xcd\x85","\xbf\x87","\xc1\x89","\xaa\xab","\xb0\xdd","\x95\x96","\xa4\xa5","\x9a\x9b","\x9f\xa0","\\|",
"zZ","\x82\x81","\xae\xa3","\xa8\xa8","\xac\xac","\xb2\xb2","\xb8\xb6",",\xb7",".d","\xdf\xaf","<>","\x0\x0","\x0\x0"
};

/* India, Gujarati */
static const char main_key_in_guj[MAIN_LEN][2] =
{
"\x0\x0","\xe7\x8d","\xe8\xc5","\xe9\xe9","\xea\xea","\xeb\xeb","\xec\xec","\xed\xed","\xee\xee","\xef(","\xe6)","-\x83","\x8b\xc3",
"\xcc\x94","\xc8\x90","\xbe\x86","\xc0\x88","\xc2\x8a","\xac\xad","\xb9\x99","\x97\x98","\xa6\xa7","\x9c\x9d","\xa1\xa2","\xbc\x9e",
"\xcb\x93","\xc7\x8f","\xcd\x85","\xbf\x87","\xc1\x89","\xaa\xab","\xb0\xb0","\x95\x96","\xa4\xa5","\x9a\x9b","\x9f\xa0","\xc9\x91",
"zZ","\x82\x81","\xae\xa3","\xa8\xa8","\xb5\xb5","\xb2\xb3","\xb8\xb6",",\xb7",".d","\xaf?","<>","\x0\x0","\x0\x0"
};

/* India, Gurmukhi */
static const char main_key_in_guru[MAIN_LEN][2] =
{
"\x0\x0","gg","hh","ii","jj","kk","ll","mm","nn","o(","f)","\x0\x0","\x0\x0",
"L\x14","H\x10",">\x6","@\x8","B\xa",",-","9\x19","\x17\x18","&'","\x1c\x1d","!\"","<\x1e",
"K\x13","G\xf","M\x5","?\x7","A\x9","*+","00","\x15\x16","$%","\x1a\x1b","\x1f\x20","\\|",
"zZ","\x2p",".#","((","55","23","86",",<",".d","/?","<>","\x0\x0","\x0\x0"
};

/* India, Kannada */
static const char main_key_in_kan[MAIN_LEN][2] =
{
"\xca\x92","\xe7\xe7","\xe8\xe8","\xe9\xe9","\xea\xea","\xeb\xeb","\xec\xec","\xed\xed","\xee\xee","\xef\xef","\xe6\xe6","\x83\x83","\xc3\x8b",
"\xcc\x94","\xc8\x90","\xbe\x86","\xc0\x88","\xc2\x8a","\xac\xad","\xb9\x99","\x97\x98","\xa6\xa7","\x9c\x9d","\xa1\xa2","\xbc\x9e",
"\xcb\x93","\xc7\x8f","\xcd\x85","\xbf\x87","\xc1\x89","\xaa\xab","\xb0\xb1","\x95\x96","\xa4\xa5","\x9a\x9b","\x9f\xa0","\\|",
"\xc6\x8e","\x82\x82","\xae\xa3","\xa8\xa8","\xb5\xb4","\xb2\xb3","\xb8\xb6",",\xb7","..","\xaf@","<>","\x0\x0","\x0\x0"
};

/* India, Malayalam */
static const char main_key_in_mal[MAIN_LEN][2] =
{
"J\x12","g!","h@","i#","j$","k%","l^","m&","n*","o(","f)","-\x3","C\xb",
"L\x14","H\x10",">\x6","@\x8","B\xa",",-","9\x19","\x17\x18","&'","\x1c\x1d","!\"","\xd\x1e",
"K\x13","G\xf","M\x5","?\x7","A\x9","*+","01","\x15\x16","$%","\x1a\x1b","\x1f\x20","\\|",
"F\xe","\x2\x2",".#","((","54","23","86",",7","..","/?","<>","\x0\x0","\x0\x0"
};

/* India, Oriya */
static const char main_key_in_ori[MAIN_LEN][2] =
{
"\x0\x0","gg","hh","ii","jj","kk","ll","mm","nn","oo","ff","\x3\x3","C\xb",
"L\x14","H\x10",">\x6","@\x8","B\xa",",-","9\x19","\x17\x18","&'","\x1c\x1d","!\"","<\x1e",
"K\x13","G\xf","M\x5","?\x7","A\x9","*+","00","\x15\x16","$%","\x1a\x1b","\x1f\x20","\\|",
"\x0\x0","\x2\x1",".#","((","55","23","86",",7","..","/@","<>","\x0\x0","\x0\x0"
};

/* India, Tamil Unicode */
static const char main_key_in_tam_unicode[MAIN_LEN][2] =
{
"\x0\x0","\x83\x83","\x0\x0","\x9c\x9c","\xb7\xb7","\xb8\xb8","\xb9\xb9","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0",
"\x9e\x9e","\xb1\xb1","\xa8\xa8","\x9a\x9a","\xb5\xb5","\xb2\xb2","\xb0\xb0","\xc8\x90","\xca\xcb","\xbf\xc0","\xc1\xc2","\x0\x0",
"\xaf\xaf","\xb3\xb3","\xa9\xa9","\x95\x95","\xaa\xaa","\xbe\xb4","\xa4\xa4","\xae\xae","\x9f\x9f","\xcd\xcd","\x99\x99","\\|",
"\xa3\xa3","\x92\x93","\x89\x8a","\x8e\x8f","\xc6\xc7","\x94\xcc","\x85\x86","\x87\x88","\x0\x0","\x0\x0","<>","\x0\x0","\x0\x0"
};

/* India, Tamil TAB Typewriter */
static const char main_key_in_tam_TAB[MAIN_LEN][2] =
{
"\x0\x0","\xe7\xa7","\xfa\xa8","\xfb\xfb","\xfc\xfc","\xfd\xfd","\xfe\xfe","\xff\xff","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0",
"\xeb\xb3","\xf8\xc1","\xef\xb8","\xea\xb2","\xf5\xbe","\xf4\xbd","\xf3\xbc","\xac\xe4","\xae\xaf","\xa4\xa6","R\xa6","\x0\x0",
"\xf2\xbb","\xf7\xc0","\xf9\xc2","\xe8\xb0","\xf0\xb9","\xa2\xa3","\xee\xb6","\xf1\xba","\xec\xb4","\xf6\xbf","\xe9\xb1","\\|",
"\xed\xb5","\xe5\xe6","\xe0\xe1","\xe2\xe3","\xaa\xab","\xac\xa3","\xdc\xdd","\xde\xdf","\x0\x0","\x0\x0","<>","\x0\x0","\x0\x0"
};

/* India, Tamil TSCII Typewriter */
static const char main_key_in_tam_TSCII[MAIN_LEN][2] =
{
"\x0\x0","\xb7\xa4","\x82\xa5","\x83\x88","\x84\x89","\x85\x8a","\x86\x8b","\x87\x8c","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0",
"\xbb\x9a","\xc8\xda","\xbf\xd1","\xba\xcd","\xc5\xd7","\xc4\xd6","\xc3\xd5","\xa8\xb3","\xca\xcb","\xa2\xa3","Q\xa3","\x0\x0",
"\xc2\xd4","\xc7\xd9","\xc9\xdb","\xb8\xcc","\xc0\xd2","P\xa1","\xbe\xd0","\xc1\xd3","\xbc\xce","\xc6\xd8","\xb9\x99","\\|",
"\xbd\xcf","\xb4\xb5","\xaf\xb0","\xb1\xb2","\xa6\xa7","\xb6\xaa","\xab\xac","\xfe\xae","\x0\x0","\x0\x0","<>","\x0\x0","\x0\x0"
};

/* India, Tamil */
static const char main_key_in_tam[MAIN_LEN][2] =
{
"\xca\x92","\xe7\xe7","\xe8\xe8","\xe9\xe9","\xea\xea","\xeb\xeb","\xec\xec","\xed\xed","\xee\xee","\xef(","\xf0)","\xf1\x83","\xf2+",
"\xcc\x94","\xc8\x90","\xbe\x86","\xc0\x88","\xc2\x8a","\x0\x0","\xb9\x99","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x9e\x9e",
"\xcb\x93","\xc7\x8f","\xcd\x85","\xbf\x87","\xc1\x89","\xaa\xaa","\xb0\xb1","\x95\x95","\xa4\xa4","\x9a\x9a","\x9f\x9f","\\|",
"\xc6\x8e","\x82\x82","\xae\xa3","\xa8\xa9","\xb5\xb4","\xb2\xb3","\xb8\xb8",",\xb7",".d","\xaf?","<>","\x0\x0","\x0\x0"
};

/* India, Telugu */
static const char main_key_in_tel[MAIN_LEN][2] =
{
"J\x12","gg","hh","i#","j$","k%","l^","m&","n*","o(","f)","\x3_","C\xb",
"L\x14","H\x10",">\x6","@\x8","B\xa",",-","9\x19","\x17\x18","&'","\x1c\x1d","!\"","\x1e\x1e",
"K\x13","G\xf","M\x5","?\x7","A\x9","*+","01","\x15\x16","$%","\x1a\x1b","\x1f\x20","\\|",
"F\xe","\x2\x1",".#","((","55","23","86",",7","..","/@","<>","\x0\x0","\x0\x0"
};

/* India, Urdu */
static const char main_key_in_urd[MAIN_LEN][2] =
{
"\xd4\xd4","a!","b@","c#","\xf4$","\xf5%","f^","g&","h*","i(","`)","-_","=+",
"\xe2\xe1","H\xf9","9\xf7","1\x91","\xcay","\xd2|","!L","\xccV","G)","~O","[{","]}",
"'\"","\xd3""5","/\x88","Ap","\xaf:","\xbe-",",6","\xa9.","D\x12","\x1b:","''","\\|",
"\xd2\xd0","4\x98","\x86+","78","((","F\xba","EE","\xcN","\xd4P","/\xbf","<>","\x0\x0","\x0\x0"
};

/* Bosnia and Herzegovina */
static const char main_key_ba[MAIN_LEN][2] =
{
"`~","1!","2\"","3#","4$","5%","6&","7/","8(","9)","0=","'?","+*",
"qQ","wW","eE","rR","tT","zZ","uU","iI","oO","pP","\xb9\xa9","\xf0\xd0",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe8\xc8","\xe6\xc6","\xbe\xae",
"yY","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Bosnia and Herzegovina, Use Bosnian digraphs */
static const char main_key_ba_unicode[MAIN_LEN][2] =
{
"`~","1!","2\"","3#","4$","5%","6&","7/","8(","9)","0=","'?","+*",
"\xc9\xc8","\xcc\xcb","eE","rR","tT","zZ","uU","iI","oO","pP","\xb9\xa9","\xf0\xd0",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe8\xc8","\xe6\xc6","\xbe\xae",
"\xbe\xae","\xc6\xc5","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Bosnia and Herzegovina, US keyboard with Bosnian digraphs */
static const char main_key_ba_unicodeus[MAIN_LEN][2] =
{
"`~","1!","2\"","3#","4$","5%","6&","7/","8(","9)","0=","'?","+*",
"\xc9\xc8","\xcc\xcb","eE","rR","tT","\xbe\xae","uU","iI","oO","pP","\xb9\xa9","\xf0\xd0",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe8\xc8","\xe6\xc6","\xbe\xae",
"zZ","\xc6\xc5","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Bosnia and Herzegovina, US keyboard with Bosnian letters */
static const char main_key_ba_us[MAIN_LEN][2] =
{
"`~","1!","2\"","3#","4$","5%","6&","7/","8(","9)","0=","'?","+*",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xb9\xa9","\xf0\xd0",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe8\xc8","\xe6\xc6","\xbe\xae",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Brazil */
static const char main_key_br[MAIN_LEN][2] =
{
"'\"","1!","2@","3#","4$","5%","6W","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","QP","[{",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe7\xc7","SR","]}",
"zZ","xX","cC","vV","bB","nN","mM",",<",".>",";:","\\|","/?","\x0\x0"
};

/* Brazil, Eliminate dead keys */
static const char main_key_br_nodeadkeys[MAIN_LEN][2] =
{
"'\"","1!","2@","3#","4$","5%","6\xa8","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","'`","[{",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe7\xc7","~^","]}",
"zZ","xX","cC","vV","bB","nN","mM",",<",".>",";:","\\|","/?","\x0\x0"
};

/* Brazil with alternative 102 */
static const char main_key_br_alt_102[MAIN_LEN][2] =
{
"'\"","1!","2@","3#","4$","5%","6W","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","QP","[{",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe7\xc7","SR","]}",
"zZ","xX","cC","vV","bB","nN","mM",",<",".>",";:","<>","/?","\x0\x0"
};

/* Brazil with alternative 102, Eliminate dead keys */
static const char main_key_br_alt_102_nodeadkeys[MAIN_LEN][2] =
{
"'\"","1!","2@","3#","4$","5%","6\xa8","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","'`","[{",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe7\xc7","~^","]}",
"zZ","xX","cC","vV","bB","nN","mM",",<",".>",";:","<>","/?","\x0\x0"
};

/* Bulgaria */
static const char main_key_bg[MAIN_LEN][2] =
{
"()","1!","2?","3+","4\"","5%","6=","7:","8/","9\xa9","0\xb0","-I",".V",
",\xd9","\xd5\xf5","\xc5\xe5","\xc9\xe9","\xdb\xfb","\xdd\xfd","\xcb\xeb","\xd3\xf3","\xc4\xe4","\xda\xfa","\xc3\xe3",";\xa7",
"\xd8\xf8","\xd1\xf1","\xc1\xe1","\xcf\xef","\xd6\xf6","\xc7\xe7","\xd4\xf4","\xce\xee","\xd7\xf7","\xcd\xed","\xde\xfe","'\xf9",
"\xc0\xe0","\xca\xea","\xdf\xff","\xdc\xfc","\xc6\xe6","\xc8\xe8","\xd0\xf0","\xd2\xf2","\xcc\xec","\xc2\xe2","<>","\x0\x0","\x0\x0"
};

/* Bulgaria, Phonetic */
static const char main_key_bg_phonetic[MAIN_LEN][2] =
{
"\xde\xfe","1!","2@","3#","4$","5%","6^","7&","8*","9(","0)","-_","=+",
"\xd1\xf1","\xd7\xf7","\xc5\xe5","\xd2\xf2","\xd4\xf4","\xdf\xff","\xd5\xf5","\xc9\xe9","\xcf\xef","\xd0\xf0","\xdb\xfb","\xdd\xfd",
"\xc1\xe1","\xd3\xf3","\xc4\xe4","\xc6\xe6","\xc7\xe7","\xc8\xe8","\xca\xea","\xcb\xeb","\xcc\xec",";:","'\"","\xc0\xe0",
"\xda\xfa","\xd8\xf8","\xc3\xe3","\xd6\xf6","\xc2\xe2","\xce\xee","\xcd\xed",",<",".>","/?","<>","\x0\x0","\x0\x0"
};

/* Myanmar */
static const char main_key_mm[MAIN_LEN][2] =
{
"\x0\x0","A!","BB","CC","DD","EE","FF","GG","HH","I(","@)","-8","RV",
"**","22",",!",".$","0&","\x17\x18","\x1f\x4","\x2\x3","\x12\x13","\x7\x8","\xd\xe","\xa\x9",
"))","''","9!","-#","/%","\x15\x16","\x1b\x1b","\x0\x1","\x10\x11","\x5\x6","\xb\xc","NO",
"LM","76","\x19\xf","\x14\x14","\x17\x17","\x1c\x1c","\x1eP",",Q",".J","/\x1a","<>","\x0\x0","\x0\x0"
};

/* Canada */
static const char main_key_ca[MAIN_LEN][2] =
{
"#|","1!","2\"","3/","4$","5%","6?","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","RR","[W",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL",";:","PP","<>",
"zZ","xX","cC","vV","bB","nN","mM",",'","..","\xe9\xc9","\xab\xbb","\x0\x0","\x0\x0"
};

/* Canada, French Dvorak */
static const char main_key_ca_fr_dvorak[MAIN_LEN][2] =
{
"#|","1!","2\"","3/","4$","5%","6?","7&","8*","9(","0)","RR","[W",
"PP",",'","..","pP","yY","fF","gG","cC","rR","lL","\xe9\xc9","=+",
"aA","oO","eE","uU","iI","dD","hH","tT","nN","sS","-_","<>",
";:","qQ","jJ","kK","xX","bB","mM","wW","vV","zZ","\xab\xbb","\x0\x0","\x0\x0"
};

/* Canada, French (legacy) */
static const char main_key_ca_fr_legacy[MAIN_LEN][2] =
{
"\xb0\xb0","1!","2\"","3#","4$","5%","6?","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","RR","\xe7\xc7",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL",";:","\xe8\xc8","\xe0\xc0",
"zZ","xX","cC","vV","bB","nN","mM",",'","..","\xe9\xc9","\xf9\xd9","\x0\x0","\x0\x0"
};

/* Canada, Multilingual */
static const char main_key_ca_multix[MAIN_LEN][2] =
{
"/\\","1!","2@","3#","4$","5%","6?","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","RW","\xe7\xc7",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL",";:","\xe8\xc8","\xe0\xc0",
"zZ","xX","cC","vV","bB","nN","mM",",'",".\"","\xe9\xc9","\xf9\xd9","\x0\x0","\x0\x0"
};

/* Canada, Multilingual, second part */
static const char main_key_ca_multi_2gr[MAIN_LEN][2] =
{
"\x0\xad","\xb9\xa1","\xb2\xb2","\xb3\xa3","\xbc\xa4","\xbd\xc4","\xbe\xc5","\x0\xc6","\x0\xc9","\x0\xb1","\x0\x0","\x0\xbf","[\\",
"\x0\xd9","\xb3\xa3","\xbd\xbc","\xb6\xae","\xbc\xac","\xfb\xa5","\xfe\xfc","\xfd\xb9","\xf8\xd8","\xfe\xde","\x0X","ST",
"\xe6\xc6","\xdf\xa7","\xf0\xd0","\x0\xaa","\xbf\xbd","\xb1\xa1","32","\xa2\xa2","@?","QY","\x0Z","\\U",
"\x0\x0","\x0\x0","\xa2\xa9","\xd2\xd0","\xd3\xd1","Ij","\xb5\xba","\xaf\xd7","\xb7\xf7","\x0V","<\xa6","\x0\x0","\x0\x0"
};

/* Canada, Inuktitut */
static const char main_key_ca_ike[MAIN_LEN][2] =
{
"{u","\x95""1","I2","P3","\x83""4","f5","\x85""6","\xbb""7","\xd0""8","\xea""9",">0","-_","]=",
"\x8f\x8b","\x3""1","\x7f""F","m\x96","Nq","\xefs","\xa5u","\xc2\xa4","\xd5\xa0","(\xa6","\xa1\x5","V\x1e",
"\x91\x8d","\x5""3","\x81H","oU","P(","\xf1)","\xa7*","\xc4W","\xd7\xa2",";:","'\"","\\|",
"\x93\x90","\xa""8","\x83K","r?","U|","\xf4\xc7","\xaa\xda",",<",".>","-Y","yw","\x0\x0","\x0\x0"
};

/* Congo, Democratic Republic of the */
static const char main_key_cd[MAIN_LEN][2] =
{
"`~","&1","\x1""2","\x0""3","(4","{5","}6",")7","\x2""8","\xc""9","\x8""0","-_","=+",
"aA","wW","eE","rR","tT","yY","uU","iI","oO","pP","[\x90","*^",
"qQ","sS","dD","fF","gG","hH","jJ","kK","lL","mM","T\x86","\"\\",
"zZ","xX","cC","vV","bB","nN",",.",";:","!?","'/","\\|","\x0\x0","\x0\x0"
};

/* Czechia */
static const char main_key_cz[MAIN_LEN][2] =
{
";X","+1","\xec""2","\xb9""3","\xe8""4","\xf8""5","\xbe""6","\xfd""7","\xe1""8","\xed""9","\xe9""0","=%","QZ",
"qQ","wW","eE","rR","tT","zZ","uU","iI","oO","pP","\xfa/",")(",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf9\"","\xa7!","W'",
"yY","xX","cC","vV","bB","nN","mM",",?",".:","-_","\\|","\x0\x0","\x0\x0"
};

/* Czechia, With <|> key */
static const char main_key_cz_bksl[MAIN_LEN][2] =
{
";X","+1","\xec""2","\xb9""3","\xe8""4","\xf8""5","\xbe""6","\xfd""7","\xe1""8","\xed""9","\xe9""0","=%","QZ",
"qQ","wW","eE","rR","tT","zZ","uU","iI","oO","pP","\xfa/",")(",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf9\"","\xa7!","\\|",
"yY","xX","cC","vV","bB","nN","mM",",?",".:","-_","\\|","\x0\x0","\x0\x0"
};

/* Czechia, qwerty */
static const char main_key_cz_qwerty[MAIN_LEN][2] =
{
";X","+1","\xec""2","\xb9""3","\xe8""4","\xf8""5","\xbe""6","\xfd""7","\xe1""8","\xed""9","\xe9""0","=%","QZ",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xfa/",")(",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf9\"","\xa7!","W'",
"zZ","xX","cC","vV","bB","nN","mM",",?",".:","-_","\\|","\x0\x0","\x0\x0"
};

/* Czechia, qwerty, extended Backslash */
static const char main_key_cz_qwerty_bksl[MAIN_LEN][2] =
{
";X","+1","\xec""2","\xb9""3","\xe8""4","\xf8""5","\xbe""6","\xfd""7","\xe1""8","\xed""9","\xe9""0","=%","QZ",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xfa/",")(",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf9\"","\xa7!","\\|",
"zZ","xX","cC","vV","bB","nN","mM",",?",".:","-_","\\|","\x0\x0","\x0\x0"
};

/* Denmark */
static const char main_key_dk[MAIN_LEN][2] =
{
"\xbd\xa7","1!","2\"","3#","4\xa4","5%","6&","7/","8(","9)","0=","+?","QP",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xe5\xc5","WR",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe6\xc6","\xf8\xd8","'*",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Denmark, Eliminate dead keys */
static const char main_key_dk_nodeadkeys[MAIN_LEN][2] =
{
"\xbd\xa7","1!","2\"","3#","4\xa4","5%","6&","7/","8(","9)","0=","+?","\xb4`",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xe5\xc5","\xa8^",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe6\xc6","\xf8\xd8","'*",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Netherlands */
static const char main_key_nl[MAIN_LEN][2] =
{
"@\xa7","1!","2\"","3#","4$","5%","6&","7_","8(","9)","0'","/?","\xb0S",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","WR","*|",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","+\xb1","'`","<>",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-=","][","\x0\x0","\x0\x0"
};

/* Bhutan */
static const char main_key_bt[MAIN_LEN][2] =
{
"\x9\xa","!\x4","\"\x5","#\x6","$H","%p","&\x8","'8","(4",")<","\x20=","\x14\x7f","\xd\x11",
"@\x90","A\x91","B\x92","D\x94","r\x80","t\x84","z{","|}","E\x95","F\x96","G\x97","I\x99",
"O\x9f","P\xa0","Q\xa1","S\xa3","T\xa4","U\xa5","V\xa6","X\xa8","Y\xa9","Z\xaa","[\xab","]\xad",
"^\xae","_\xaf","`q","a\xb1","b\xb2","c\xb3","d\xb4","f\xb6","g\xb7","h\xb8","\xd\x11","\x0\x0","\x0\x0"
};

/* Estonia */
static const char main_key_ee[MAIN_LEN][2] =
{
"ZS","1!","2\"","3#","4\xa4","5%","6&","7/","8(","9)","0=","+?","QP",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xfc\xdc","\xf5\xd5",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf6\xd6","\xe4\xc4","'*",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Estonia, Eliminate dead keys */
static const char main_key_ee_nodeadkeys[MAIN_LEN][2] =
{
"^~","1!","2\"","3#","4\xa4","5%","6&","7/","8(","9)","0=","+?","'`",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xfc\xdc","\xf5\xd5",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf6\xd6","\xe4\xc4","'*",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Iran */
static const char main_key_ir[MAIN_LEN][2] =
{
"\xd\xf7","\xf1!","\xf2l","\xf3k","\xf4\xfc","\xf5j","\xf6\xd7","\xf7\xac","\xf8*","\xf9)","\xf0(","-\xe0","=+",
"\xd6\xf2","\xd5\xec","\xcb\xed","\xe2\xeb","\xe1\xef","\xda\xf0","\xd9\xee","\xe7\xf1","\xce]","\xcd[","\xcc}","\x86{",
"\xd4\xc4","\xd3\xc6","\xcc\xea","\xc8\xc5","\xe4\xc3","\xc7\xc2","\xca\xc9","\xe6\xbb","\xe5\xab","\xa9:","\xaf\xbb","\\|",
"\xd8\xe3","\xd7S","\xd2\x98","\xd1p","\xd0\xc","\xcfT","~\xc1","\xe8>",".<","/\xbf","<>","\x0\x0","\x0\x0"
};

/* Iran, Kurdish, Latin Q */
static const char main_key_ir_ku[MAIN_LEN][2] =
{
"\"\\","1!","2'","3^","4+","5%","6&","7/","8(","9)","0=","*?","-_",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","xX","\xfb\xdb",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xba\xaa","\xee\xce",",;",
"zZ","xX","cC","vV","bB","nN","mM","\xea\xca","\xe7\xc7",".:","<>","\x0\x0","\x0\x0"
};

/* Iran, Kurdish, (F) */
static const char main_key_ir_ku_f[MAIN_LEN][2] =
{
"+*","1!","2\"","3^","4$","5%","6&","7'","8(","9)","0=","/?","-_",
"fF","gG","xX","iI","oO","dD","rR","nN","hH","pP","qQ","wW",
"\xfb\xdb","\xee\xce","eE","aA","uU","tT","kK","mM","lL","yY","\xba\xaa","xX",
"jJ","\xea\xca","vV","cC","\xe7\xc7","zZ","sS","bB",".:",",;","<>","\x0\x0","\x0\x0"
};

/* Iran, Kurdish, Arabic-Latin */
static const char main_key_ir_ku_ara[MAIN_LEN][2] =
{
"\xd\xf7","1!","2@","3#","4$","5%","6^","7&","8*","9)","0(","-\xe0","=+",
"\xe2X","\xe8X","\xd5\xe7","\xd1\x95","\xca\xd7","\xcc\xce","\xc6\xc1","\xcd\xd9","\xc6\xc4","~\xcb","]}","[{",
"\xc7\xc2","\xd3\xd4","\xcf\xd0","\xe1\xc5","\xaf\xda","\xe7\xc","\x98\xc3","\xa9\xe3","\xe4\xb5","\xbb:","'\"","\\|",
"\xd2\xd6","\xce\xd5","\xcc\x86","\xa4\xd8","\xc8I","\xe6\xc9","\xe5\xe0","\xac>",".<","/\xbf","<>","\x0\x0","\x0\x0"
};

/* Faroe Islands */
static const char main_key_fo[MAIN_LEN][2] =
{
"\xbd\xa7","1!","2\"","3#","4\xa4","5%","6&","7/","8(","9)","0=","+?","QP",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xe5\xc5","\xf0\xd0",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe6\xc6","\xf8\xd8","'*",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Finland */
static const char main_key_fi[MAIN_LEN][2] =
{
"\xa7\xbd","1!","2\"","3#","4\xa4","5%","6&","7/","8(","9)","0=","+?","QP",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xe5\xc5","WR",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf6\xd6","\xe4\xc4","'*",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Finland, Eliminate dead keys */
static const char main_key_fi_nodeadkeys[MAIN_LEN][2] =
{
"\xa7\xbd","1!","2\"","3#","4\xa4","5%","6&","7/","8(","9)","0=","+?","\xb4`",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xe5\xc5","\xa8^",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf6\xd6","\xe4\xc4","'*",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Finland, Northern Saami */
static const char main_key_fi_smi[MAIN_LEN][2] =
{
"|\xa7","1!","2\"","3#","4$","5%","6&","7/","8(","9)","0=","+?","\\`",
"\xe1\xc1","\xb9\xa9","eE","rR","tT","yY","uU","iI","oO","pP","\xe5\xc5","\xbf\xbd",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf6\xd6","\xe4\xc4","\xf0\xd0",
"zZ","\xe8\xc8","cC","vV","bB","nN","mM",",;",".:","-_","\xbe\xae","\x0\x0","\x0\x0"
};

/* Finland, Macintosh */
static const char main_key_fi_mac[MAIN_LEN][2] =
{
"\xa7\xb0","1!","2\"","3#","4\xa4","5%","6&","7/","8(","9)","0=","+?","\xb4`",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xe5\xc5","\xa8^",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf6\xd6","\xe4\xc4","'*",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* France */
static const char main_key_fr[MAIN_LEN][2] =
{
"\xb2~","&1","\xe9""2","\"3","'4","(5","-6","\xe8""7","_8","\xe7""9","\xe0""0",")\xb0","=+",
"aA","zZ","eE","rR","tT","yY","uU","iI","oO","pP","RW","$\xa3",
"qQ","sS","dD","fF","gG","hH","jJ","kK","lL","mM","\xf9%","*\xb5",
"wW","xX","cC","vV","bB","nN",",?",";.",":/","!\xa7","<>","\x0\x0","\x0\x0"
};

/* France, Eliminate dead keys */
static const char main_key_fr_nodeadkeys[MAIN_LEN][2] =
{
"\xb2~","&1","\xe9""2","\"3","'4","(5","-6","\xe8""7","_8","\xe7""9","\xe0""0",")\xb0","=+",
"aA","zZ","eE","rR","tT","yY","uU","iI","oO","pP","^\xa8","$\xa3",
"qQ","sS","dD","fF","gG","hH","jJ","kK","lL","mM","\xf9%","*\xb5",
"wW","xX","cC","vV","bB","nN",",?",";.",":/","!\xa7","<>","\x0\x0","\x0\x0"
};

/* France, Sun dead keys */
static const char main_key_fr_sundeadkeys[MAIN_LEN][2] =
{
"\xb2~","&1","\xe9""2","\"3","'4","(5","-6","\xe8""7","_8","\xe7""9","\xe0""0",")\xb0","=+",
"aA","zZ","eE","rR","tT","yY","uU","iI","oO","pP","\x1\x4","$\xa3",
"qQ","sS","dD","fF","gG","hH","jJ","kK","lL","mM","\xf9%","*\xb5",
"wW","xX","cC","vV","bB","nN",",?",";.",":/","!\xa7","<>","\x0\x0","\x0\x0"
};

/* France, Alternative */
static const char main_key_fr_oss[MAIN_LEN][2] =
{
"\xf8\xd8","&1","\xe9""2","\"3","'4","(5","-6","\xe8""7","_8","\xe7""9","\xe0""0",")\xb0","=+",
"aA","zZ","eE","rR","tT","yY","uU","iI","oO","pP","RW","$\xa3",
"qQ","sS","dD","fF","gG","hH","jJ","kK","lL","mM","\xf9%","*\xb5",
"wW","xX","cC","vV","bB","nN",",?",";.",":/","!\xa7","<>","\x0\x0","\x0\x0"
};

/* France, Alternative, eliminate dead keys */
static const char main_key_fr_oss_nodeadkeys[MAIN_LEN][2] =
{
"\xf8\xd8","&1","\xe9""2","\"3","'4","(5","-6","\xe8""7","_8","\xe7""9","\xe0""0",")\xb0","=+",
"aA","zZ","eE","rR","tT","yY","uU","iI","oO","pP","^\xa8","$\xa3",
"qQ","sS","dD","fF","gG","hH","jJ","kK","lL","mM","\xf9%","*\xb5",
"wW","xX","cC","vV","bB","nN",",?",";.",":/","!\xa7","<>","\x0\x0","\x0\x0"
};

/* France, Alternative, Sun dead keys */
static const char main_key_fr_oss_sundeadkeys[MAIN_LEN][2] =
{
"\xf8\xd8","&1","\xe9""2","\"3","'4","(5","-6","\xe8""7","_8","\xe7""9","\xe0""0",")\xb0","=+",
"aA","zZ","eE","rR","tT","yY","uU","iI","oO","pP","\x1\x4","$\xa3",
"qQ","sS","dD","fF","gG","hH","jJ","kK","lL","mM","\xf9%","*\xb5",
"wW","xX","cC","vV","bB","nN",",?",";.",":/","!\xa7","<>","\x0\x0","\x0\x0"
};

/* France, (Legacy) Alternative */
static const char main_key_fr_latin9[MAIN_LEN][2] =
{
"\xbd\xbc","&1","\xe9""2","\"3","'4","(5","-6","\xe8""7","_8","\xe7""9","\xe0""0",")\xb0","=+",
"aA","zZ","eE","rR","tT","yY","uU","iI","oO","pP","RW","$\xa3",
"qQ","sS","dD","fF","gG","hH","jJ","kK","lL","mM","\xf9%","*\xb5",
"wW","xX","cC","vV","bB","nN",",?",";.",":/","!\xa7","<>","\x0\x0","\x0\x0"
};

/* France, (Legacy) Alternative, eliminate dead keys */
static const char main_key_fr_latin9_nodeadkeys[MAIN_LEN][2] =
{
"\xbd\xbc","&1","\xe9""2","\"3","'4","(5","-6","\xe8""7","_8","\xe7""9","\xe0""0",")\xb0","=+",
"aA","zZ","eE","rR","tT","yY","uU","iI","oO","pP","^\xa8","$\xa3",
"qQ","sS","dD","fF","gG","hH","jJ","kK","lL","mM","\xf9%","*\xb5",
"wW","xX","cC","vV","bB","nN",",?",";.",":/","!\xa7","<>","\x0\x0","\x0\x0"
};

/* France, (Legacy) Alternative, Sun dead keys */
static const char main_key_fr_latin9_sundeadkeys[MAIN_LEN][2] =
{
"\xbd\xbc","&1","\xe9""2","\"3","'4","(5","-6","\xe8""7","_8","\xe7""9","\xe0""0",")\xb0","=+",
"aA","zZ","eE","rR","tT","yY","uU","iI","oO","pP","\x1\x4","$\xa3",
"qQ","sS","dD","fF","gG","hH","jJ","kK","lL","mM","\xf9%","*\xb5",
"wW","xX","cC","vV","bB","nN",",?",";.",":/","!\xa7","<>","\x0\x0","\x0\x0"
};

/* France, Dvorak */
static const char main_key_fr_dvorak[MAIN_LEN][2] =
{
"\xbd\xbc","/1","+2","-3","*4","=5","\\6","(7","`8",")9","\"0","[{","]}",
":?","\xe0\xc0","\xe9\xc9","gG",".!","hH","vV","cC","mM","kK","\xe8\xc8","zZ",
"oO","aA","uU","eE","bB","fF","sS","tT","nN","dD","wW","\xf9\xd9",
"'_","qQ",",;","iI","yY","xX","rR","lL","pP","jJ","\xe7\xc7","\x0\x0","\x0\x0"
};

/* France, Macintosh */
static const char main_key_fr_mac[MAIN_LEN][2] =
{
"@#","&1","\xe9""2","\"3","'4","(5","\xa7""6","\xe8""7","!8","\xe7""9","\xe0""0",")\xb0","-_",
"aA","zZ","eE","rR","tT","yY","uU","iI","oO","pP","RW","$*",
"qQ","sS","dD","fF","gG","hH","jJ","kK","lL","mM","\xf9%","P\xa3",
"wW","xX","cC","vV","bB","nN",",?",";.",":/","=+","<>","\x0\x0","\x0\x0"
};

/* Ghana */
static const char main_key_gh[MAIN_LEN][2] =
{
"`~","1!","2@","3#","4\xb5","5%","6^","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","[{","]}",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL",";:","'\"","\\|",
"zZ","xX","cC","vV","bB","nN","mM",",<",".>","/?","<>","\x0\x0","\x0\x0"
};

/* Ghana, Akan */
static const char main_key_gh_akan[MAIN_LEN][2] =
{
"`~","1!","2@","3#","4\xb5","5%","6^","7&","8*","9(","0)","-_","=+",
"[\x90","wW","eE","rR","tT","yY","uU","iI","oO","pP","[{","]}",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL",";:","'\"","\\|",
"zZ","T\x86","cC","vV","bB","nN","mM",",<",".>","/?","<>","\x0\x0","\x0\x0"
};

/* Ghana, Ewe */
static const char main_key_gh_ewe[MAIN_LEN][2] =
{
"`~","1!","2@","3#","4\xb5","5%","6^","7&","8*","9(","0)","-_","=+",
"[\x90","wW","eE","rR","tT","yY","uU","iI","oO","pP","T\x86","\x8b\xb2",
"aA","sS","dD","fF","gG","hH","\x92\x91","kK","lL",";:","'\"","\\|",
"zZ","xX","KJ","vV","bB","nN","mM",",<",".>","/?","<>","\x0\x0","\x0\x0"
};

/* Ghana, Fula */
static const char main_key_gh_fula[MAIN_LEN][2] =
{
"`~","1!","2@","3#","4\xb5","5%","6^","7&","8*","9(","0)","-_","=+",
"qQ","\xfc\xdc","eE","rR","tT","yY","uU","iI","oO","pP","\xb4\xb3","\xe7\xe6",
"aA","sS","W\x8a","fF","gG","hH","jJ","kK","lL","10","\xdd\x8e","\\|",
"r\x9d","xX","cC","vV","bB","nN","\xf1\xd1",",<","\xe7\xc7","\xba\xaa","<>","\x0\x0","\x0\x0"
};

/* Ghana, Ga */
static const char main_key_gh_ga[MAIN_LEN][2] =
{
"`~","1!","2@","3#","4\xb5","5%","6^","7&","8*","9(","0)","-_","=+",
"[\x90","wW","eE","rR","tT","yY","uU","iI","oO","pP","[{","]}",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL",";:","'\"","\\|",
"zZ","T\x86","KJ","vV","bB","nN","mM",",<",".>","/?","<>","\x0\x0","\x0\x0"
};

/* Georgia */
static const char main_key_ge[MAIN_LEN][2] =
{
"\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0",
"\xe5q","\xec\xed","\xd4""e","\xe0\xe6","\xe2\xd7","\xe7y","\xe3u","\xd8i","\xddo","\xdep","[{","]}",
"\xd0""a","\xe1\xe8","\xd3""d","\xe4""f","\xd2g","\xf0h","\xef\xdf","\xd9k","\xdal",";:","'\"","\\|",
"\xd6\xeb","\xeex","\xea\xe9","\xd5v","\xd1""b","\xdcn","\xdbm",",<",".>","/?","\xab\xbb","\x0\x0","\x0\x0"
};

/* Georgia, Russian */
static const char main_key_ge_ru[MAIN_LEN][2] =
{
"^~","1!","2@","3#","4;","5:","6,","7.","8*","9(","0)","-_","#|",
"\xe6q","\xeaw","\xe3""e","\xd9r","\xd4t","\xdcy","\xd2u","\xe8i","\xeco","\xd6p","\xee[","\xef]",
"\xe4""a","\xd7s","\xd5""d","\xd0""f","\xdeg","\xe0h","\xddj","\xdak","\xd3l","\xdf;","\xeb%","\\|",
"\xedz","\xe9x","\xe1""c","\xdbv","\xd8""b","\xe2n","\xe5m","\xd1<","\xe7>","\xf0?","\xab\xbb","\x0\x0","\x0\x0"
};

/* Germany */
static const char main_key_de[MAIN_LEN][2] =
{
"R\xb0","1!","2\"","3\xa7","4$","5%","6&","7/","8(","9)","0=","\xdf?","QP",
"qQ","wW","eE","rR","tT","zZ","uU","iI","oO","pP","\xfc\xdc","+*",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf6\xd6","\xe4\xc4","#'",
"yY","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Germany, Dead acute */
static const char main_key_de_deadacute[MAIN_LEN][2] =
{
"^\xb0","1!","2\"","3\xa7","4$","5%","6&","7/","8(","9)","0=","\xdf?","Q`",
"qQ","wW","eE","rR","tT","zZ","uU","iI","oO","pP","\xfc\xdc","+*",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf6\xd6","\xe4\xc4","#'",
"yY","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Germany, Dead grave acute */
static const char main_key_de_deadgraveacute[MAIN_LEN][2] =
{
"^\xb0","1!","2\"","3\xa7","4$","5%","6&","7/","8(","9)","0=","\xdf?","QP",
"qQ","wW","eE","rR","tT","zZ","uU","iI","oO","pP","\xfc\xdc","+*",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf6\xd6","\xe4\xc4","#'",
"yY","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Germany, Eliminate dead keys */
static const char main_key_de_nodeadkeys[MAIN_LEN][2] =
{
"^\xb0","1!","2\"","3\xa7","4$","5%","6&","7/","8(","9)","0=","\xdf?","\xb4`",
"qQ","wW","eE","rR","tT","zZ","uU","iI","oO","pP","\xfc\xdc","+*",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf6\xd6","\xe4\xc4","#'",
"yY","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Germany, Eliminate dead keys, acute replaced by apostrophe */
static const char main_key_de_nodeadkeys_noacute[MAIN_LEN][2] =
{
"^\xb0","1!","2\"","3\xa7","4$","5%","6&","7/","8(","9)","0=","\xdf?","\x27`",
"qQ","wW","eE","rR","tT","zZ","uU","iI","oO","pP","\xfc\xdc","+*",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf6\xd6","\xe4\xc4","#'",
"yY","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Germany, Dvorak */
static const char main_key_de_dvorak[MAIN_LEN][2] =
{
"^\xb0","1!","2\"","3\xa7","4$","5%","6&","7/","8(","9)","0=","+*","<>",
"\xfc\xdc",",;",".:","pP","yY","fF","gG","cC","tT","zZ","?\xdf","/\\",
"aA","oO","eE","iI","uU","hH","dD","rR","nN","sS","lL","-_",
"\xf6\xd6","qQ","jJ","kK","xX","bB","mM","wW","vV","#'","\xe4\xc4","\x0\x0","\x0\x0"
};

/* Germany, Neostyle */
static const char main_key_de_neo[MAIN_LEN][2] =
{
"R\xb0","1!","2\"","3\xb6","4$","5%","6&","7/","8(","9)","0=","-_","QP",
"qQ","vV","lL","cC","wW","kK","hH","gG","fF","jJ","\xdf?","+*",
"uU","iI","aA","eE","oO","sS","nN","rR","tT","dD","yY","#'",
"\xf6\xd6","\xfc\xdc","\xe4\xc4","pP","zZ","bB","mM",",;",".:","xX","<>","\x0\x0","\x0\x0"
};

/* Greece */
static const char main_key_gr[MAIN_LEN][2] =
{
"`~","1!","2@","3#","4$","5%","6^","7&","8*","9(","0)","-_","=+",
";:","\xf3\xd2","\xe5\xc5","\xf1\xd1","\xf4\xd4","\xf5\xd5","\xe8\xc8","\xe9\xc9","\xef\xcf","\xf0\xd0","[{","]}",
"\xe1\xc1","\xf2\xd2","\xe4\xc4","\xf6\xd6","\xe3\xc3","\xe7\xc7","\xee\xce","\xea\xca","\xeb\xcb","QW","'\"","\\|",
"\xe6\xc6","\xf7\xd7","\xf8\xd8","\xf9\xd9","\xe2\xc2","\xed\xcd","\xec\xcc",",<",".>","/?","\xab\xbb","\x0\x0","\x0\x0"
};

/* Greece, Eliminate dead keys */
static const char main_key_gr_nodeadkeys[MAIN_LEN][2] =
{
"`~","1!","2@","3#","4$","5%","6^","7&","8*","9(","0)","-_","=+",
";:","\xf3\xd2","\xe5\xc5","\xf1\xd1","\xf4\xd4","\xf5\xd5","\xe8\xc8","\xe9\xc9","\xef\xcf","\xf0\xd0","[{","]}",
"\xe1\xc1","\xf2\xd2","\xe4\xc4","\xf6\xd6","\xe3\xc3","\xe7\xc7","\xee\xce","\xea\xca","\xeb\xcb",";:","'\"","\\|",
"\xe6\xc6","\xf7\xd7","\xf8\xd8","\xf9\xd9","\xe2\xc2","\xed\xcd","\xec\xcc",",<",".>","/?","\xab\xbb","\x0\x0","\x0\x0"
};

/* Greece, Polytonic */
static const char main_key_gr_polytonic[MAIN_LEN][2] =
{
"`~","1!","2@","3#","4$","5%","6^","7&","8*","9(","0)","-_","=+",
";:","\xf3\xd2","\xe5\xc5","\xf1\xd1","\xf4\xd4","\xf5\xd5","\xe8\xc8","\xe9\xc9","\xef\xcf","\xf0\xd0","SW","]\xff",
"\xe1\xc1","\xf2\xd2","\xe4\xc4","\xf6\xd6","\xe3\xc3","\xe7\xc7","\xee\xce","\xea\xca","\xeb\xcb","Q\x13","P\x14","\\|",
"\xe6\xc6","\xf7\xd7","\xf8\xd8","\xf9\xd9","\xe2\xc2","\xed\xcd","\xec\xcc",",<",".>","/?","\xab\xbb","\x0\x0","\x0\x0"
};

/* Hungary */
static const char main_key_hu[MAIN_LEN][2] =
{
"0\xa7","1'","2\"","3+","4!","5%","6/","7=","8(","9)","\xf6\xd6","\xfc\xdc","\xf3\xd3",
"qQ","wW","eE","rR","tT","zZ","uU","iI","oO","pP","\xf5\xd5","\xfa\xda",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe9\xc9","\xe1\xc1","\xfb\xdb",
"yY","xX","cC","vV","bB","nN","mM",",?",".:","-_","\xed\xcd","\x0\x0","\x0\x0"
};

/* Hungary, qwerty */
static const char main_key_hu_qwerty[MAIN_LEN][2] =
{
"\xed\xcd","1'","2\"","3+","4!","5%","6/","7=","8(","9)","\xf6\xd6","\xfc\xdc","\xf3\xd3",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xf5\xd5","\xfa\xda",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe9\xc9","\xe1\xc1","\xfb\xdb",
"zZ","xX","cC","vV","bB","nN","mM",",?",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Hungary, 101/qwertz/comma/Dead keys */
static const char main_key_hu_101_qwertz_comma_dead[MAIN_LEN][2] =
{
"\xed\xcd","1'","2\"","3+","4!","5%","6/","7=","8(","9)","\xf6\xd6","\xfc\xdc","\xf3\xd3",
"qQ","wW","eE","rR","tT","zZ","uU","iI","oO","pP","\xf5\xd5","\xfa\xda",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe9\xc9","\xe1\xc1","\xfb\xdb",
"yY","xX","cC","vV","bB","nN","mM",",?",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Hungary, 102/qwerty/comma/Dead keys */
static const char main_key_hu_102_qwerty_comma_dead[MAIN_LEN][2] =
{
"0\xa7","1'","2\"","3+","4!","5%","6/","7=","8(","9)","\xf6\xd6","\xfc\xdc","\xf3\xd3",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xf5\xd5","\xfa\xda",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe9\xc9","\xe1\xc1","\xfb\xdb",
"zZ","xX","cC","vV","bB","nN","mM",",?",".:","-_","\xed\xcd","\x0\x0","\x0\x0"
};

/* Iceland */
static const char main_key_is[MAIN_LEN][2] =
{
"\xb0\xa8","1!","2\"","3#","4$","5%","6&","7/","8(","9)","0=","\xf6\xd6","-_",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xf0\xd0","'?",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe6\xc6","Q\xc4","+*",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","\xfe\xde","<>","\x0\x0","\x0\x0"
};

/* Iceland, Sun dead keys */
static const char main_key_is_Sundeadkeys[MAIN_LEN][2] =
{
"\x1\xb0","1!","2\"","3#","4$","5%","6&","7/","8(","9)","0=","\xf6\xd6","\x3\x0",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xfc\xdc","+*",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf6\xd6","\xe4\xc4","#\xb4",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","\xfe\xde","<>","\x0\x0","\x0\x0"
};

/* Iceland, Eliminate dead keys */
static const char main_key_is_nodeadkeys[MAIN_LEN][2] =
{
"^\xb0","1!","2\"","3#","4$","5%","6&","7/","8(","9)","0=","\xf6\xd6","'`",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xfc\xdc","+*",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf6\xd6","\xe4\xc4","#\xb4",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","\xfe\xde","<>","\x0\x0","\x0\x0"
};

/* Iceland, Macintosh */
static const char main_key_is_mac[MAIN_LEN][2] =
{
"\xa3\xa7","1!","2\"","3#","4$","5%","6&","7/","8(","9)","0=","\xf6\xd6","-_",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xf0\xd0","'?",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe6\xc6","QW","+*",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","\xfe\xde","<>","\x0\x0","\x0\x0"
};

/* Israel */
static const char main_key_il[MAIN_LEN][2] =
{
";~","1!","2@","3#","4$","5%","6^","7&","8*","9)","0(","-_","=+",
"/Q","'W","\xf7""E","\xf8R","\xe0T","\xe8Y","\xe5U","\xefI","\xedO","\xf4P","]}","[{",
"\xf9""A","\xe3S","\xe2""D","\xeb""F","\xf2G","\xe9H","\xe7J","\xecK","\xeaL","\xf3:",",\"","\\\\",
"\xe6Z","\xf1X","\xe1""C","\xe4V","\xf0""B","\xeeN","\xf6M","\xfa>","\xf5<",".?","<>","\x0\x0","\x0\x0"
};

/* Israel, lyx */
static const char main_key_il_lyx[MAIN_LEN][2] =
{
";~","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","9)","0(","-\xbe","\x0\x0",
"//","''","\xf7\xb8","\xf8\xbc","\xe0\xe","\xe8\xf","\xe5\xb9","\xef\xef","\xed\xed","\xf4\xb7","]}","[{",
"\xf9\xb0","\xe3\xbc","\xe2\xe2","\xeb\xeb","\xf2\xc2","\xe9\xc1","\xe7\xb4","\xec\xaa","\xea\xea","\xf3:",",\"","\\|",
"\xe6\xe6","\xf1\xb6","\xe1\xbb","\xe4\xb1","\xf0\xb2","\xee\xb3","\xf6\xb5","\xfa>","\xf5<",".?","<>","\x0\x0","\x0\x0"
};

/* Israel, Phonetic */
static const char main_key_il_phonetic[MAIN_LEN][2] =
{
"\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","=+",
"\xf7\xf7","\xe5\xe5","\xe0\xe0","\xf8\xf8","\xfa\xe8","\xf2\xf2","\xe5\xe5","\xe9\xe9","\xf1\xf1","\xf4\xf3","\x0\x0","\x0\x0",
"\xe0\xe0","\xf9\xf9","\xe3\xe3","\xf4\xf3","\xe2\xe2","\xe4\xe4","\xe9\xe9","\xeb\xea","\xec\xec","\x0\x0","\x0\x0","\\|",
"\xe6\xe6","\xe7\xe7","\xf6\xf5","\xe5\xe5","\xe1\xe1","\xf0\xef","\xee\xed","\x0\x0","\x0\x0","\x0\x0","<>","\x0\x0","\x0\x0"
};

/* Italy */
static const char main_key_it[MAIN_LEN][2] =
{
"\\|","1!","2\"","3\xa3","4$","5%","6&","7/","8(","9)","0=","'?","\xec^",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xe8\xe9","+*",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf2\xe7","\xe0\xb0","\xf9\xa7",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Italy, Macintosh */
static const char main_key_it_mac[MAIN_LEN][2] =
{
"@#","&1","\"2","'3","(4","\xe7""5","\xe8""6",")7","\xa3""8","\xe0""9","\xe9""0","-_","=+",
"qQ","zZ","eE","rR","tT","yY","uU","iI","oO","pP","\xec^","$*",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","mM","\xf9%","\xa7\xb0",
"wW","xX","cC","vV","bB","nN",",?",";.",":/","\xf2!","<>","\x0\x0","\x0\x0"
};

/* Japan */
static const char main_key_jp[MAIN_LEN][2] =
{
"*!","1!","2\"","3#","4$","5%","6&","7'","8(","9)","0~","-=","^~",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","@`","[{",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL",";+",":*","]}",
"zZ","xX","cC","vV","bB","nN","mM",",<",".>","/?","<>","\\_","\\|"
};

/* Kyrgyzstan */
static const char main_key_kg[MAIN_LEN][2] =
{
"\xa3\xb3","1!","2\"","3\xb0","4;","5%","6:","7?","8*","9(","0)","-_","=+",
"\xca\xea","\xc3\xe3","\xd5\xf5","\xcb\xeb","\xc5\xe5","\xce\xee","\xc7\xe7","\xdb\xfb","\xdd\xfd","\xda\xfa","\xc8\xe8","\xdf\xff",
"\xc6\xe6","\xd9\xf9","\xd7\xf7","\xc1\xe1","\xd0\xf0","\xd2\xf2","\xcf\xef","\xcc\xec","\xc4\xe4","\xd6\xf6","\xdc\xfc","\\/",
"\xd1\xf1","\xde\xfe","\xd3\xf3","\xcd\xed","\xc9\xe9","\xd4\xf4","\xd8\xf8","\xc2\xe2","\xc0\xe0",".,","/|","\x0\x0","\x0\x0"
};

/* Cambodia */
static const char main_key_kh[MAIN_LEN][2] =
{
"\xab\xbb","\xe1!","\xe2\xd7","\xe3\"","\xe4\xdb","\xe5%","\xe6\xcd","\xe7\xd0","\xe8\xcf","\xe9(","\xe0)","\xa5\xcc","\xb2=",
"\x86\x88","\xb9\xba","\xc1\xc2","\x9a\xac","\x8f\x91","\x99\xbd","\xbb\xbc","\xb7\xb8","\xc4\xc5","\x95\x97","\xc0\xbf","\xaa\xa7",
"\xb6\xff","\x9f\xc3","\x8a\x8c","\x90\x92","\x84\xa2","\xa0\xc7","\xd2\x89","\x80\x82","\x9b\xa1","\xbe\xfe","\xcb\xc9","\xae\xad",
"\x8b\x8d","\x81\x83","\x85\x87","\x9c\xfd","\x94\x96","\x93\x8e","\x98\xc6","\xfc\xfb","\xd4\xd5","\xca?","<>","\x0\x0","\x0\x0"
};

/* Kazakhstan */
static const char main_key_kz[MAIN_LEN][2] =
{
"()","\xfe!","\xd9\xd8","\xa6\xb6","\xa3\xa2","\x93\x92",",;",".:","\xaf\xae","\xb1\xb0","\x9b\x9a","\xe9\xe8","\xbb\xba",
"\xca\xea","\xc3\xe3","\xd5\xf5","\xcb\xeb","\xc5\xe5","\xce\xee","\xc7\xe7","\xdb\xfb","\xdd\xfd","\xda\xfa","\xc8\xe8","\xdf\xff",
"\xc6\xe6","\xd9\xf9","\xd7\xf7","\xc1\xe1","\xd0\xf0","\xd2\xf2","\xcf\xef","\xcc\xec","\xc4\xe4","\xd6\xf6","\xdc\xfc","\\/",
"\xd1\xf1","\xde\xfe","\xd3\xf3","\xcd\xed","\xc9\xe9","\xd4\xf4","\xd8\xf8","\xc2\xe2","\xc0\xe0","\xb0?","<>","\x0\x0","\x0\x0"
};

/* Kazakhstan, Russian with Kazakh */
static const char main_key_kz_ruskaz[MAIN_LEN][2] =
{
"\xa3\xb3","1!","2\"","3\xb0","4;","5%","6:","7?","8*","9(","0)","-_","=+",
"\xca\xea","\xc3\xe3","\xd5\xf5","\xcb\xeb","\xc5\xe5","\xce\xee","\xc7\xe7","\xdb\xfb","\xdd\xfd","\xda\xfa","\xc8\xe8","\xdf\xff",
"\xc6\xe6","\xd9\xf9","\xd7\xf7","\xc1\xe1","\xd0\xf0","\xd2\xf2","\xcf\xef","\xcc\xec","\xc4\xe4","\xd6\xf6","\xdc\xfc","\\/",
"\xd1\xf1","\xde\xfe","\xd3\xf3","\xcd\xed","\xc9\xe9","\xd4\xf4","\xd8\xf8","\xc2\xe2","\xc0\xe0",".,","<>","\x0\x0","\x0\x0"
};

/* Kazakhstan, Kazakh with Russian */
static const char main_key_kz_kazrus[MAIN_LEN][2] =
{
"\xa3\xb3","1!","2\"","3\xb0","4;","5%","6:","7?","8*","9(","0)","-_","=+",
"\xca\xea","\xa3\xa2","\xd5\xf5","\xcb\xeb","\xc5\xe5","\xce\xee","\xc7\xe7","\xdb\xfb","\x93\x92","\xda\xfa","\xc8\xe8","\xb1\xb0",
"\xe9\xe8","\xd9\xf9","\x9b\x9a","\xc1\xe1","\xd0\xf0","\xd2\xf2","\xcf\xef","\xcc\xec","\xc4\xe4","\xd6\xf6","\xd9\xd8","\\/",
"\xd1\xf1","\xa6\xb6","\xd3\xf3","\xcd\xed","\xc9\xe9","\xd4\xf4","\xaf\xae","\xc2\xe2","\xc0\xe0",".,","<>","\x0\x0","\x0\x0"
};

/* Laos */
static const char main_key_la[MAIN_LEN][2] =
{
"\xd\xd","\xa2\xd1","\x9f\xd2","\xc2\xd3","\x96\xd4","\xb8\xcc","\xb9\xbc","\x84\xd5","\x95\xd6","\x88\xd7","\x82\xd8","\x8a\xd9","\xcd\xcd",
"\xbb\xbb","\xc4\xd0","\xb3\xb3","\x9e_","\xb0+","\xb4\xb4","\xb5\xb5","\xa3\xae","\x99\x99","\x8d\xbd","\x9a-","\xa5}",
"\xb1\xb1","\xab;","\x81.","\x94,","\xc0:","\xc9\xca","\xc8\xcb","\xb2!","\xaa?","\xa7%","\x87=","\xdc\xdd",
"\x9c\xad","\x9b(","\xc1\xaf","\xad\xad","\xb6\xb6","\xb7\xb7","\x97\xc6","\xa1`","\xc3$","\x9d)","<>","\x0\x0","\x0\x0"
};

/* Latin American */
static const char main_key_latam[MAIN_LEN][2] =
{
"|\xb0","1!","2\"","3#","4$","5%","6&","7/","8(","9)","0=","'?","\xbf\xa1",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","QW","+*",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf1\xd1","{[","}]",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Latin American, Eliminate dead keys */
static const char main_key_latam_nodeadkeys[MAIN_LEN][2] =
{
"|\xb0","1!","2\"","3#","4$","5%","6&","7/","8(","9)","0=","'?","\xbf\xa1",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","`^","+*",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf1\xd1","\xb4\xa8","\xe7\xc7",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Latin American, Sun dead keys */
static const char main_key_latam_sundeadkeys[MAIN_LEN][2] =
{
"|\xb0","1!","2\"","3#","4$","5%","6&","7/","8(","9)","0=","'?","\xbf\xa1",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\x0\x1","+*",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf1\xd1","\x3\x4","}]",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Lithuania */
static const char main_key_lt[MAIN_LEN][2] =
{
"`~","\xb1\xa1","\xe8\xc8","\xea\xca","\xec\xcc","\xe7\xc7","\xb9\xa9","\xf9\xd9","\xfe\xde","\xfe(","\xd2)","-_","\xbe\xae",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","[{","]}",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL",";:","'\"","\\|",
"zZ","xX","cC","vV","bB","nN","mM",",<",".>","/?","\xaa\xac","\x0\x0","\x0\x0"
};

/* Lithuania, Standard */
static const char main_key_lt_std[MAIN_LEN][2] =
{
"`~","!1","-2","/3",";4",":5",",6",".7","=8","(9",")0","?+","xX",
"\xb1\xa1","\xbe\xae","eE","rR","tT","yY","uU","iI","oO","pP","\xe7\xc7","wW",
"aA","sS","dD","\xb9\xa9","gG","hH","jJ","kK","lL","\xf9\xd9","\xec\xcc","qQ",
"zZ","\xfe\xde","cC","vV","bB","nN","mM","\xe8\xc8","fF","\xea\xca","<>","\x0\x0","\x0\x0"
};

/* Lithuania, US keyboard with Lithuanian letters */
static const char main_key_lt_us[MAIN_LEN][2] =
{
"`~","1!","2@","3#","4$","5%","6^","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","[{","]}",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL",";:","'\"","\\|",
"zZ","xX","cC","vV","bB","nN","mM",",<",".>","/?","\xaa\xac","\x0\x0","\x0\x0"
};

/* Lithuania, IBM (LST 1205-92) */
static const char main_key_lt_ibm[MAIN_LEN][2] =
{
"`~","!1","\"2","/3",";4",":5",",6",".7","?8","(9",")0","_-","+=",
"\xb1\xa1","\xbe\xae","eE","rR","tT","yY","uU","iI","oO","pP","\xe7\xc7","\xfe\xd2",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf9\xd9","\xec\xcc","\\|",
"zZ","\xfe\xde","cC","vV","bB","nN","mM","\xe8\xc8","\xb9\xa9","\xea\xca","<>","\x0\x0","\x0\x0"
};

/* Latvia, Apostrophe (') variant */
static const char main_key_lv_apostrophe[MAIN_LEN][2] =
{
"`~","1!","2@","3#","4$","5%","6^","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","[{","]}",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL",";:","\x4\"","\\|",
"zZ","xX","cC","vV","bB","nN","mM",",<",".>","/?","<>","\x0\x0","\x0\x0"
};

/* Latvia, Tilde (~) variant */
static const char main_key_lv_tilde[MAIN_LEN][2] =
{
"\x4~","1!","2@","3#","4$","5%","6^","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","[{","]}",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL",";:","'\"","\\|",
"zZ","xX","cC","vV","bB","nN","mM",",<",".>","/?","<>","\x0\x0","\x0\x0"
};

/* Latvia, F-letter (F) variant */
static const char main_key_lv_fkey[MAIN_LEN][2] =
{
"`~","1!","2@","3#","4$","5%","6^","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","[{","]}",
"aA","sS","dD","\x4\x4","gG","hH","jJ","kK","lL",";:","'\"","\\|",
"zZ","xX","cC","vV","bB","nN","mM",",<",".>","/?","<>","\x0\x0","\x0\x0"
};

/* Macedonia */
static const char main_key_mk[MAIN_LEN][2] =
{
"P~","1!","2\xfe","3\xd2","4$","5%","6^","7&","8*","9(","0)","-_","=+",
"\xa9\xb9","\xaa\xba","\xc5\xe5","\xd2\xf2","\xd4\xf4","\xa5\xb5","\xd5\xf5","\xc9\xe9","\xcf\xef","\xd0\xf0","\xdb\xfb","\xa2\xb2",
"\xc1\xe1","\xd3\xf3","\xc4\xe4","\xc6\xe6","\xc7\xe7","\xc8\xe8","\xa8\xb8","\xcb\xeb","\xcc\xec","\xde\xfe","\xac\xbc","\xd6\xf6",
"\xda\xfa","\xaf\xbf","\xc3\xe3","\xd7\xf7","\xc2\xe2","\xce\xee","\xcd\xed",",;",".:","/?","<>","\x0\x0","\x0\x0"
};

/* Macedonia, Eliminate dead keys */
static const char main_key_mk_nodeadkeys[MAIN_LEN][2] =
{
"`~","1!","2\xfe","3\xd2","4$","5%","6^","7&","8*","9(","0)","-_","=+",
"\xa9\xb9","\xaa\xba","\xc5\xe5","\xd2\xf2","\xd4\xf4","\xa5\xb5","\xd5\xf5","\xc9\xe9","\xcf\xef","\xd0\xf0","\xdb\xfb","\xa2\xb2",
"\xc1\xe1","\xd3\xf3","\xc4\xe4","\xc6\xe6","\xc7\xe7","\xc8\xe8","\xa8\xb8","\xcb\xeb","\xcc\xec","\xde\xfe","\xac\xbc","\xd6\xf6",
"\xda\xfa","\xaf\xbf","\xc3\xe3","\xd7\xf7","\xc2\xe2","\xce\xee","\xcd\xed",",;",".:","/?","<>","\x0\x0","\x0\x0"
};

/* Malta */
static const char main_key_mt[MAIN_LEN][2] =
{
"\xe5\xc5","1!","2@","3\xac","4$","5%","6^","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xf5\xd5","\xb1\xa1",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL",";:","'\"","\\|",
"zZ","xX","cC","vV","bB","nN","mM",",<",".>","/?","\xbf\xaf","\x0\x0","\x0\x0"
};

/* Malta, Maltese keyboard with US layout */
static const char main_key_mt_us[MAIN_LEN][2] =
{
"\xe5\xc5","1!","2@","3\xac","4$","5%","6^","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xf5\xd5","\xb1\xa1",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL",";:","'\"","\xbf\xaf",
"zZ","xX","cC","vV","bB","nN","mM",",<",".>","/?","\xbf\xaf","\x0\x0","\x0\x0"
};

/* Mongolia */
static const char main_key_mn[MAIN_LEN][2] =
{
"=+","1\xb0","2-","3\"","4\xae","5:","6.","7_","8,","9%","0?","\xc5\xe5","\xdd\xfd",
"\xc6\xe6","\xc3\xe3","\xd5\xf5","\xd6\xf6","\xdc\xfc","\xce\xee","\xc7\xe7","\xdb\xfb","\xaf\xae","\xda\xfa","\xcb\xeb","\xdf\xff",
"\xca\xea","\xd9\xf9","\xc2\xe2","\xe9\xe8","\xc1\xe1","\xc8\xe8","\xd2\xf2","\xcf\xef","\xcc\xec","\xc4\xe4","\xd0\xf0","!|",
"\xd1\xf1","\xde\xfe","\xa3\xb3","\xd3\xf3","\xcd\xed","\xc9\xe9","\xd4\xf4","\xd8\xf8","\xd7\xf7","\xc0\xe0","()","\x0\x0","\x0\x0"
};

/* Norway */
static const char main_key_no[MAIN_LEN][2] =
{
"|\xa7","1!","2\"","3#","4\xa4","5%","6&","7/","8(","9)","0=","+?","\\P",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xe5\xc5","WR",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf8\xd8","\xe6\xc6","'*",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Norway, Eliminate dead keys */
static const char main_key_no_nodeadkeys[MAIN_LEN][2] =
{
"|\xa7","1!","2\"","3#","4\xa4","5%","6&","7/","8(","9)","0=","+?","\\`",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xe5\xc5","\xa8^",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf8\xd8","\xe6\xc6","'*",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Norway, Dvorak */
static const char main_key_no_dvorak[MAIN_LEN][2] =
{
"|\xa7","1!","2\"","3#","4$","5%","6&","7/","8(","9)","0=","+?","\\`",
"\xe5\xc5",",;",".:","pP","yY","fF","gG","cC","rR","lL","'*","~^",
"aA","oO","eE","uU","iI","dD","hH","tT","nN","sS","-_","<>",
"\xe6\xc6","qQ","jJ","kK","xX","bB","mM","wW","vV","zZ","\xf8\xd8","\x0\x0","\x0\x0"
};

/* Norway, Northern Saami */
static const char main_key_no_smi[MAIN_LEN][2] =
{
"|\xa7","1!","2\"","3#","4$","5%","6&","7/","8(","9)","0=","+?","\\`",
"\xe1\xc1","\xb9\xa9","eE","rR","tT","yY","uU","iI","oO","pP","\xe5\xc5","\xbf\xbd",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf8\xd8","\xe6\xc6","\xf0\xd0",
"zZ","\xe8\xc8","cC","vV","bB","nN","mM",",;",".:","-_","\xbe\xae","\x0\x0","\x0\x0"
};

/* Norway, Macintosh */
static const char main_key_no_mac[MAIN_LEN][2] =
{
"|\xa7","1!","2\"","3#","4\xa4","5%","6&","7/","8(","9)","0=","+?","PQ",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xe5\xc5","\xa8^",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf8\xd8","\xe6\xc6","@*",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Norway, Macintosh, eliminate dead keys */
static const char main_key_no_mac_nodeadkeys[MAIN_LEN][2] =
{
"|\xa7","1!","2\"","3#","4\xa4","5%","6&","7/","8(","9)","0=","+?","`\xb4",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xe5\xc5","\xa8^",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf8\xd8","\xe6\xc6","@*",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Poland, qwertz */
static const char main_key_pl_qwertz[MAIN_LEN][2] =
{
"\xff\\","1!","2\"","3#","4$","5%","6&","7/","8(","9)","0=","+?","'*",
"qQ","wW","eE","rR","tT","zZ","uU","iI","oO","pP","\xbf\xf1","\xb6\xe6",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xb3\xa3","\xb1\xea","\xf3\xbc",
"yY","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Portugal */
static const char main_key_pt[MAIN_LEN][2] =
{
"\\|","1!","2\"","3#","4$","5%","6&","7/","8(","9)","0=","'?","\xab\xbb",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","+*","QP",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe7\xc7","\xba\xaa","SR",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Portugal, Eliminate dead keys */
static const char main_key_pt_nodeadkeys[MAIN_LEN][2] =
{
"\\|","1!","2\"","3#","4$","5%","6&","7/","8(","9)","0=","'?","\xab\xbb",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","+*","\xb4`",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe7\xc7","\xba\xaa","~^",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Portugal, Sun dead keys */
static const char main_key_pt_sundeadkeys[MAIN_LEN][2] =
{
"\\|","1!","2\"","3#","4$","5%","6&","7/","8(","9)","0=","'?","\xab\xbb",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","+*","\x3\x0",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe7\xc7","\xba\xaa","\x2\x1",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Portugal, Macintosh */
static const char main_key_pt_mac[MAIN_LEN][2] =
{
"`~","1!","2\"","3#","4$","5%","6&","7/","8(","9)","0=","'?","+*",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xba\xaa","QP",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe7\xc7","SR","\\|",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Portugal, Macintosh, eliminate dead keys */
static const char main_key_pt_mac_nodeadkeys[MAIN_LEN][2] =
{
"`~","1!","2\"","3#","4$","5%","6&","7/","8(","9)","0=","'?","+*",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xba\xaa","\xb4`",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe7\xc7","~^","\\|",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Portugal, Macintosh, Sun dead keys */
static const char main_key_pt_mac_sundeadkeys[MAIN_LEN][2] =
{
"`~","1!","2\"","3#","4$","5%","6&","7/","8(","9)","0=","'?","+*",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xba\xaa","\x3\x0",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe7\xc7","\x2\x1","\\|",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Romania */
static const char main_key_ro[MAIN_LEN][2] =
{
"`~","1!","2@","3#","4$","5%","6^","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","[{","]}",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL",";:","'\"","\\|",
"zZ","xX","cC","vV","bB","nN","mM",",<",".>","/?","\\|","\x0\x0","\x0\x0"
};

/* Romania, Standard */
static const char main_key_ro_std[MAIN_LEN][2] =
{
"\xfe\xd3","1!","2@","3#","4$","5%","6^","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xe3\xc3","\xee\xce",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xba\xaa","\xfe\xde","\xe2\xc2",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","/?","\\|","\x0\x0","\x0\x0"
};

/* Romania, Standard (Commabelow) */
static const char main_key_ro_academic[MAIN_LEN][2] =
{
"\xfe\xd3","1!","2@","3#","4$","5%","6^","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xe3\xc3","\xee\xce",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\x19\x18","\x1b\x1a","\xe2\xc2",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","/?","\\|","\x0\x0","\x0\x0"
};

/* Romania, Winkeys */
static const char main_key_ro_winkeys[MAIN_LEN][2] =
{
"][","1!","2\"","3#","4\xa4","5%","6&","7/","8(","9)","0=","+?","'*",
"qQ","wW","eE","rR","tT","zZ","uU","iI","oO","pP","\xe3\xc3","\xee\xce",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xba\xaa","\xfe\xde","\xe2\xc2",
"yY","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Russia */
static const char main_key_ru[MAIN_LEN][2] =
{
"\xa3\xb3","1!","2\"","3#","4*","5:","6,","7.","8;","9(","0)","-_","=+",
"\xca\xea","\xc3\xe3","\xd5\xf5","\xcb\xeb","\xc5\xe5","\xce\xee","\xc7\xe7","\xdb\xfb","\xdd\xfd","\xda\xfa","\xc8\xe8","\xdf\xff",
"\xc6\xe6","\xd9\xf9","\xd7\xf7","\xc1\xe1","\xd0\xf0","\xd2\xf2","\xcf\xef","\xcc\xec","\xc4\xe4","\xd6\xf6","\xdc\xfc","\\|",
"\xd1\xf1","\xde\xfe","\xd3\xf3","\xcd\xed","\xc9\xe9","\xd4\xf4","\xd8\xf8","\xc2\xe2","\xc0\xe0","/?","/|","\x0\x0","\x0\x0"
};

/* Russia, Phonetic */
static const char main_key_ru_phonetic[MAIN_LEN][2] =
{
"\xc0\xe0","1!","2@","3\xa3","4\xb3","5\xdf","6\xff","7&","8*","9(","0)","\x0\x0","\xde\xfe",
"\xd1\xf1","\xd7\xf7","\xc5\xe5","\xd2\xf2","\xd4\xf4","\xd9\xf9","\xd5\xf5","\xc9\xe9","\xcf\xef","\xd0\xf0","\xdb\xfb","\xdd\xfd",
"\xc1\xe1","\xd3\xf3","\xc4\xe4","\xc6\xe6","\xc7\xe7","\xc8\xe8","\xca\xea","\xcb\xeb","\xcc\xec",";:","'\"","\xdc\xfc",
"\xda\xfa","\xd8\xf8","\xc3\xe3","\xd6\xf6","\xc2\xe2","\xce\xee","\xcd\xed",",<",".>","/?","|\xa6","\x0\x0","\x0\x0"
};

/* Russia, Typewriter */
static const char main_key_ru_typewriter[MAIN_LEN][2] =
{
"'\"","!1","\xb0""2","/3",";4",":5",",6",".7","_8","?9","%0","-_","=+",
"\xca\xea","\xc3\xe3","\xd5\xf5","\xcb\xeb","\xc5\xe5","\xce\xee","\xc7\xe7","\xdb\xfb","\xdd\xfd","\xda\xfa","\xc8\xe8","\xdf\xff",
"\xc6\xe6","\xd9\xf9","\xd7\xf7","\xc1\xe1","\xd0\xf0","\xd2\xf2","\xcf\xef","\xcc\xec","\xc4\xe4","\xd6\xf6","\xdc\xfc","()",
"\xd1\xf1","\xde\xfe","\xd3\xf3","\xcd\xed","\xc9\xe9","\xd4\xf4","\xd8\xf8","\xc2\xe2","\xc0\xe0","\xa3\xb3","/|","\x0\x0","\x0\x0"
};

/* Russia, Tatar */
static const char main_key_ru_tt[MAIN_LEN][2] =
{
"\xbb\xba","1!","2\"","3\xb0","4;","5%","6:","7?","8*","9(","0)","-_","=+",
"\xca\xea","\xe9\xe8","\xd5\xf5","\xcb\xeb","\xc5\xe5","\xce\xee","\xc7\xe7","\xdb\xfb","\xd9\xd8","\xda\xfa","\xc8\xe8","\xaf\xae",
"\xc6\xe6","\xd9\xf9","\xd7\xf7","\xc1\xe1","\xd0\xf0","\xd2\xf2","\xcf\xef","\xcc\xec","\xc4\xe4","\xa3\xa2","\xdc\xfc","\\/",
"\xd1\xf1","\xde\xfe","\xd3\xf3","\xcd\xed","\xc9\xe9","\xd4\xf4","\x97\x96","\xc2\xe2","\xc0\xe0",".,","/|","\x0\x0","\x0\x0"
};

/* Russia, Ossetian */
static const char main_key_ru_os[MAIN_LEN][2] =
{
"\xdc\xfc","1!","2\"","3#","4*","5:","6,","7.","8;","9(","0)","-_","=+",
"\xca\xea","\xc3\xe3","\xd5\xf5","\xcb\xeb","\xc5\xe5","\xce\xee","\xc7\xe7","\xdb\xfb","\xdd\xfd","\xda\xfa","\xc8\xe8","\xdf\xff",
"\xc6\xe6","\xd9\xf9","\xd7\xf7","\xc1\xe1","\xd0\xf0","\xd2\xf2","\xcf\xef","\xcc\xec","\xc4\xe4","\xd6\xf6","\xd5\xd4","\\|",
"\xd1\xf1","\xde\xfe","\xd3\xf3","\xcd\xed","\xc9\xe9","\xd4\xf4","\xd8\xf8","\xc2\xe2","\xc0\xe0","/?","/|","\x0\x0","\x0\x0"
};

/* Russia, Ossetian, Winkeys */
static const char main_key_ru_os_winkeys[MAIN_LEN][2] =
{
"\xdc\xfc","1!","2\"","3\xb0","4;","5%","6:","7?","8*","9(","0)","-_","=+",
"\xca\xea","\xc3\xe3","\xd5\xf5","\xcb\xeb","\xc5\xe5","\xce\xee","\xc7\xe7","\xdb\xfb","\xdd\xfd","\xda\xfa","\xc8\xe8","\xdf\xff",
"\xc6\xe6","\xd9\xf9","\xd7\xf7","\xc1\xe1","\xd0\xf0","\xd2\xf2","\xcf\xef","\xcc\xec","\xc4\xe4","\xd6\xf6","\xd5\xd4","\\/",
"\xd1\xf1","\xde\xfe","\xd3\xf3","\xcd\xed","\xc9\xe9","\xd4\xf4","\xd8\xf8","\xc2\xe2","\xc0\xe0",".,","/|","\x0\x0","\x0\x0"
};

/* Serbia and Montenegro */
static const char main_key_cs[MAIN_LEN][2] =
{
"`~","1!","2\"","3#","4$","5%","6&","7/","8(","9)","0=","'?","+*",
"\xa9\xb9","\xaa\xba","\xc5\xe5","\xd2\xf2","\xd4\xf4","\xda\xfa","\xd5\xf5","\xc9\xe9","\xcf\xef","\xd0\xf0","\xdb\xfb","\xa1\xb1",
"\xc1\xe1","\xd3\xf3","\xc4\xe4","\xc6\xe6","\xc7\xe7","\xc8\xe8","\xa8\xb8","\xcb\xeb","\xcc\xec","\xde\xfe","\xab\xbb","\xd6\xf6",
"\xd6\xf6","\xaf\xbf","\xc3\xe3","\xd7\xf7","\xc2\xe2","\xce\xee","\xcd\xed",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Serbia and Montenegro, Z and ZHE swapped */
static const char main_key_cs_yz[MAIN_LEN][2] =
{
"`~","1!","2\"","3#","4$","5%","6&","7/","8(","9)","0=","'?","+*",
"\xa9\xb9","\xaa\xba","\xc5\xe5","\xd2\xf2","\xd4\xf4","\xd6\xf6","\xd5\xf5","\xc9\xe9","\xcf\xef","\xd0\xf0","\xdb\xfb","\xa1\xb1",
"\xc1\xe1","\xd3\xf3","\xc4\xe4","\xc6\xe6","\xc7\xe7","\xc8\xe8","\xa8\xb8","\xcb\xeb","\xcc\xec","\xde\xfe","\xab\xbb","\xd6\xf6",
"\xda\xfa","\xaf\xbf","\xc3\xe3","\xd7\xf7","\xc2\xe2","\xce\xee","\xcd\xed",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Slovakia */
static const char main_key_sk[MAIN_LEN][2] =
{
";X","+1","\xb5""2","\xb9""3","\xe8""4","\xbb""5","\xbe""6","\xfd""7","\xe1""8","\xed""9","\xe9""0","=%","QZ",
"qQ","wW","eE","rR","tT","zZ","uU","iI","oO","pP","\xfa/","\xe4(",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf4\"","\xa7!","\xf2)",
"yY","xX","cC","vV","bB","nN","mM",",?",".:","-_","\\|","\x0\x0","\x0\x0"
};

/* Slovakia, Extended Backslash */
static const char main_key_sk_bksl[MAIN_LEN][2] =
{
";X","+1","\xb5""2","\xb9""3","\xe8""4","\xbb""5","\xbe""6","\xfd""7","\xe1""8","\xed""9","\xe9""0","=%","QZ",
"qQ","wW","eE","rR","tT","zZ","uU","iI","oO","pP","\xfa/","\xe4(",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf4\"","\xa7!","\\|",
"yY","xX","cC","vV","bB","nN","mM",",?",".:","-_","\\|","\x0\x0","\x0\x0"
};

/* Slovakia, qwerty */
static const char main_key_sk_qwerty[MAIN_LEN][2] =
{
";X","+1","\xb5""2","\xb9""3","\xe8""4","\xbb""5","\xbe""6","\xfd""7","\xe1""8","\xed""9","\xe9""0","=%","QZ",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xfa/","\xe4(",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf4\"","\xa7!","\xf2)",
"zZ","xX","cC","vV","bB","nN","mM",",?",".:","-_","\\|","\x0\x0","\x0\x0"
};

/* Slovakia, qwerty, extended Backslash */
static const char main_key_sk_qwerty_bksl[MAIN_LEN][2] =
{
";X","+1","\xb5""2","\xb9""3","\xe8""4","\xbb""5","\xbe""6","\xfd""7","\xe1""8","\xed""9","\xe9""0","=%","QZ",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xfa/","\xe4(",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf4\"","\xa7!","\\|",
"zZ","xX","cC","vV","bB","nN","mM",",?",".:","-_","\\|","\x0\x0","\x0\x0"
};

/* Spain */
static const char main_key_es[MAIN_LEN][2] =
{
"\xba\xaa","1!","2\"","3\xb7","4$","5%","6&","7/","8(","9)","0=","'?","\xa1\xbf",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","PR","+*",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf1\xd1","QW","\xe7\xc7",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Spain, Eliminate dead keys */
static const char main_key_es_nodeadkeys[MAIN_LEN][2] =
{
"\xba\xaa","1!","2\"","3\xb7","4$","5%","6&","7/","8(","9)","0=","'?","\xa1\xbf",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","`^","+*",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf1\xd1","\xb4\xa8","\xe7\xc7",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Spain, Sun dead keys */
static const char main_key_es_sundeadkeys[MAIN_LEN][2] =
{
"\xba\xaa","1!","2\"","3\xb7","4$","5%","6&","7/","8(","9)","0=","'?","\xa1\xbf",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\x0\x1","+*",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf1\xd1","\x3\x4","\xe7\xc7",
"zZ","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Spain, Dvorak */
static const char main_key_es_dvorak[MAIN_LEN][2] =
{
"\xba\xaa","1!","2\"","3\xb7","4$","5%","6&","7/","8(","9)","0=","'?","\xa1\xbf",
".:",",;","\xf1\xd1","pP","yY","fF","gG","cC","hH","lL","PR","+*",
"aA","oO","eE","uU","iI","dD","rR","tT","nN","sS","QW","\xe7\xc7",
"-_","qQ","jJ","kK","xX","bB","mM","wW","vV","zZ","<>","\x0\x0","\x0\x0"
};

/* Sweden, Dvorak */
static const char main_key_se_dvorak[MAIN_LEN][2] =
{
"\xa7\xbd","1!","2\"","3#","4\xa4","5%","6&","7/","8(","9)","0=","+?","QP",
"\xe5\xc5","\xe4\xc4","\xf6\xd6","pP","yY","fF","gG","cC","rR","lL",",;","WR",
"aA","oO","eE","uU","iI","dD","hH","tT","nN","sS","-_","'*",
".:","qQ","jJ","kK","xX","bB","mM","wW","vV","zZ","<>","\x0\x0","\x0\x0"
};

/* Sweden, Russian phonetic */
static const char main_key_se_rus[MAIN_LEN][2] =
{
"\xa3\xb3","1!","2\"","3#","4\xa4","5%","6&","7/","8(","9)","0=","+?","\xdf\xff",
"\xd1\xf1","\xd7\xf7","\xc5\xe5","\xd2\xf2","\xd4\xf4","\xd9\xf9","\xd5\xf5","\xc9\xe9","\xcf\xef","\xd0\xf0","\xdc\xfc","WR",
"\xc1\xe1","\xd3\xf3","\xc4\xe4","\xc6\xe6","\xc7\xe7","\xc8\xe8","\xca\xea","\xcb\xeb","\xcc\xec","\xdb\xfb","\xdd\xfd","\xc0\xe0",
"\xda\xfa","\xd8\xf8","\xc3\xe3","\xd6\xf6","\xc2\xe2","\xce\xee","\xcd\xed",",;",".:","-_","\xde\xfe","\x0\x0","\x0\x0"
};

/* Switzerland */
static const char main_key_ch[MAIN_LEN][2] =
{
"\xa7\xb0","1+","2\"","3*","4\xe7","5%","6&","7/","8(","9)","0=","'?","RP",
"qQ","wW","eE","rR","tT","zZ","uU","iI","oO","pP","\xfc\xe8","W!",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf6\xe9","\xe4\xe0","$\xa3",
"yY","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Switzerland, German, eliminate dead keys */
static const char main_key_ch_de_nodeadkeys[MAIN_LEN][2] =
{
"\xa7\xb0","1+","2\"","3*","4\xe7","5%","6&","7/","8(","9)","0=","'?","^`",
"qQ","wW","eE","rR","tT","zZ","uU","iI","oO","pP","\xfc\xe8","\xa8!",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf6\xe9","\xe4\xe0","$\xa3",
"yY","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Switzerland, German, Sun dead keys */
static const char main_key_ch_de_sundeadkeys[MAIN_LEN][2] =
{
"\xa7\xb0","1+","2\"","3*","4\xe7","5%","6&","7/","8(","9)","0=","'?","\x1\x0",
"qQ","wW","eE","rR","tT","zZ","uU","iI","oO","pP","\xfc\xe8","\x4!",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xf6\xe9","\xe4\xe0","$\xa3",
"yY","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Switzerland, French */
static const char main_key_ch_fr[MAIN_LEN][2] =
{
"\xa7\xb0","1+","2\"","3*","4\xe7","5%","6&","7/","8(","9)","0=","'?","RP",
"qQ","wW","eE","rR","tT","zZ","uU","iI","oO","pP","\xe8\xfc","W!",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe9\xf6","\xe0\xe4","$\xa3",
"yY","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Switzerland, French, eliminate dead keys */
static const char main_key_ch_fr_nodeadkeys[MAIN_LEN][2] =
{
"\xa7\xb0","1+","2\"","3*","4\xe7","5%","6&","7/","8(","9)","0=","'?","^`",
"qQ","wW","eE","rR","tT","zZ","uU","iI","oO","pP","\xe8\xfc","\xa8!",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe9\xf6","\xe0\xe4","$\xa3",
"yY","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Switzerland, French, Sun dead keys */
static const char main_key_ch_fr_sundeadkeys[MAIN_LEN][2] =
{
"\xa7\xb0","1+","2\"","3*","4\xe7","5%","6&","7/","8(","9)","0=","'?","\x1\x0",
"qQ","wW","eE","rR","tT","zZ","uU","iI","oO","pP","\xe8\xfc","\x4!",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe9\xf6","\xe0\xe4","$\xa3",
"yY","xX","cC","vV","bB","nN","mM",",;",".:","-_","<>","\x0\x0","\x0\x0"
};

/* Syria, Syriac */
static const char main_key_sy_syc[MAIN_LEN][2] =
{
"\xf.","1!","2\xa","3%","4I","5p","6q","7\xa","8\xbb","9)","0(","-\xab","=+",
"\x14""0","(3","\x16""6","):","&=","\x1c@","%A","\x17\x8","\x1e\x4","\x1a\x7","\x13\x3","\x15J",
"+1","#4","\x1d""7","\x12;","\x20>","\x10\x11",",\xe0","\"$","!1","\x1f#","\x1b""0","\x6:",
"]2","[5","$8","*<","'?","\x0""9",".B","\x18\xac","\x19\xbb","\x7\xbf","<>","\x0\x0","\x0\x0"
};

/* Syria, Syriac phonetic */
static const char main_key_sy_syc_phonetic[MAIN_LEN][2] =
{
"\xf.","1!","2\xa","3%","4I","5p","6q","7\xa","8\xbb","9)","0(","-\xab","=+",
")0","\x18""3","\x16""6","*:",",=","\x1d@","\x1c""A","%\x8","'\x4","&\x7","]\x3","[J",
"\x10""1","#4","\x15""7","\x14;","\x13>","\x17\x11","\x1b\xe0","\x1f$","\x20""1","\x1a#","\x1e""0","\x6:",
"\x19""2","(5","$8","+<","\x12?","\"9","!B","\x0\xac",".\xbb","\x7\xbf","<>","\x0\x0","\x0\x0"
};

/* Tajikistan */
static const char main_key_tj[MAIN_LEN][2] =
{
"\xa3\xb3","1!","2\"","3'","4*","5:","6,","7.","8;","9(","0)","-_","=+",
"\xca\xea","\xc3\xe3","\xd5\xf5","\xcb\xeb","\xc5\xe5","\xce\xee","\xc7\xe7","\xdb\xfb","[T","\xda\xfa","\xc8\xe8","\xdf\xff",
"\xc6\xe6","\xd9\xf9","\xd7\xf7","\xc1\xe1","\xd0\xf0","\xd2\xf2","\xcf\xef","\xcc\xec","\xc4\xe4","\xd6\xf6","\xdc\xfc","\\|",
"\xd1\xf1","\xde\xfe","\xd3\xf3","\xcd\xed","\xc9\xe9","\xd4\xf4","\xd8\xf8","\xc2\xe2","\xc0\xe0","/?","|\xa6","\x0\x0","\x0\x0"
};

/* Sri Lanka */
static const char main_key_lk[MAIN_LEN][2] =
{
"`~","1!","2@","3#","4$","5%","6^","7&","8*","9(","0)","-_","=+",
"\x8d\x8e","\x87\x88","\x91\x92","\xbb\xca","\xa7\xa8","\xba\xca","\x8b\x8c","\x89\x8a","\x94\x95","\xb4\xb5","[{","]}",
"\x85\x86","\xc3\xc2","\xa9\xaa","\xc6""F","\x9c\x9d","\xc4\x83","\xa2\xa3","\x9a\x9b","\xbd\xc5",";:","'\"","\\|",
"\xa4\xa5","\xac\xb3","\xa0\xa1","\xc0V","\xb6\xb7","\xb1\xab","\xb8\xb9",",<",".>","/?","<>","\x0\x0","\x0\x0"
};

/* Thailand */
static const char main_key_th[MAIN_LEN][2] =
{
"_%","\xe5+","/\xf1","-\xf2","\xc0\xf3","\xb6\xf4","\xd8\xd9","\xd6\xdf","\xa4\xf5","\xb5\xf6","\xa8\xf7","\xa2\xf8","\xaa\xf9",
"\xe6\xf0","\xe4\"","\xd3\xae","\xbe\xb1","\xd0\xb8","\xd1\xed","\xd5\xea","\xc3\xb3","\xb9\xcf","\xc2\xad","\xba\xb0","\xc5,",
"\xbf\xc4","\xcb\xa6","\xa1\xaf","\xb4\xe2","\xe0\xac","\xe9\xe7","\xe8\xeb","\xd2\xc9","\xca\xc8","\xc7\xab","\xa7.","\xa3\xa5",
"\xbc(","\xbb)","\xe1\xa9","\xcd\xce","\xd4\xda","\xd7\xec","\xb7?","\xc1\xb2","\xe3\xcc","\xbd\xc6","<>","\x0\x0","\x0\x0"
};

/* Thailand, TIS-820.2538 */
static const char main_key_th_tis[MAIN_LEN][2] =
{
"O[","\xdf\xe5","/\xf1","-\xf2","\xc0\xf3","\xb6\xf4","\xd8\xd9","\xd6N","\xa4\xf5","\xb5\xf6","\xa8\xf7","\xa2\xf8","\xaa\xf9",
"\xe6\xf0","\xe4\"","\xd3\xae","\xbe\xb1","\xd0\xb8","\xd1\xed","\xd5\xea","\xc3\xb3","\xb9\xcf","\xc2\xad","\xba\xb0","\xc5,",
"\xbf\xc4","\xcb\xa6","\xa1\xaf","\xb4\xe2","\xe0\xac","\xe9\xe7","\xe8\xeb","\xd2\xc9","\xca\xc8","\xc7\xab","\xa7.","\xa5\xa3",
"\xbc(","\xbb)","\xe1\xa9","\xcd\xce","\xd4\xda","\xd7\xec","\xb7?","\xc1\xb2","\xe3\xcc","\xbd\xc6","<>","\x0\x0","\x0\x0"
};

/* Thailand, Pattachote */
static const char main_key_th_pat[MAIN_LEN][2] =
{
"_\xdf","=+","\xf2\"","\xf3/","\xf4,","\xf5?","\xd9\xd8","\xf7_","\xf8.","\xf9(","\xf0)","\xf1-","\xf6%",
"\xe7\xea","\xb5\xc4","\xc2\xe6","\xcd\xad","\xc3\xc9","\xe8\xd6","\xb4\xbd","\xc1\xab","\xc7\xb6","\xe1\xb2","\xe3\xcf","\xac\xc6",
"\xe9\xeb","\xb7\xb8","\xa7\xd3","\xa1\xb3","\xd1\xec","\xd5\xd7","\xd2\xbc","\xb9\xaa","\xe0\xe2","\xe4\xa6","\xa2\xb1","\xe5\xed",
"\xba\xae","\xbb\xaf","\xc5\xb0","\xcb\xc0","\xd4\xda","\xa4\xc8","\xca\xce","\xd0\xbf","\xa8\xa9","\xbe\xcc","<>","\x0\x0","\x0\x0"
};

/* Turkey */
static const char main_key_tr[MAIN_LEN][2] =
{
"\"\\","1!","2'","3^","4+","5%","6&","7/","8(","9)","0=","*?","-_",
"qQ","wW","eE","rR","tT","yY","uU","\xb9I","oO","pP","\xbb\xab","\xfc\xdc",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xba\xaa","i\xa9",",;",
"zZ","xX","cC","vV","bB","nN","mM","\xf6\xd6","\xe7\xc7",".:","<>","\x0\x0","\x0\x0"
};

/* Turkey, (F) */
static const char main_key_tr_f[MAIN_LEN][2] =
{
"+*","1!","2\"","3^","4$","5%","6&","7'","8(","9)","0=","/?","-_",
"fF","gG","\xbb\xab","\xb9I","oO","dD","rR","nN","hH","pP","qQ","wW",
"uU","i\xa9","eE","aA","\xfc\xdc","tT","kK","mM","lL","yY","\xba\xaa","xX",
"jJ","\xf6\xd6","vV","cC","\xe7\xc7","zZ","sS","bB",".:",",;","<>","\x0\x0","\x0\x0"
};

/* Ukraine */
static const char main_key_ua[MAIN_LEN][2] =
{
"'~","1!","2\"","3#","4*","5:","6,","7.","8;","9(","0)","-_","=+",
"\xca\xea","\xc3\xe3","\xd5\xf5","\xcb\xeb","\xc5\xe5","\xce\xee","\xc7\xe7","\xdb\xfb","\xdd\xfd","\xda\xfa","\xc8\xe8","\xa7\xb7",
"\xc6\xe6","\xa6\xb6","\xd7\xf7","\xc1\xe1","\xd0\xf0","\xd2\xf2","\xcf\xef","\xcc\xec","\xc4\xe4","\xd6\xf6","\xa4\xb4","\xad\xbd",
"\xd1\xf1","\xde\xfe","\xd3\xf3","\xcd\xed","\xc9\xe9","\xd4\xf4","\xd8\xf8","\xc2\xe2","\xc0\xe0","/?","/|","\x0\x0","\x0\x0"
};

/* Ukraine, Phonetic */
static const char main_key_ua_phonetic[MAIN_LEN][2] =
{
"'~","1!","2\"","3#","4*","5:","6,","7.","8;","9(","0)","-_","=+",
"\xd1\xf1","\xd7\xf7","\xc5\xe5","\xd2\xf2","\xd4\xf4","\xc9\xc9","\xd5\xf5","\xa6\xb6","\xcf\xef","\xd0\xf0","\xdb\xfb","\xdd\xfd",
"\xc1\xe1","\xd3\xf3","\xc4\xe4","\xc6\xe6","\xc7\xe7","\xc8\xe8","\xca\xea","\xcb\xeb","\xcc\xec","\xad\xbd","\xde\xfe","\xc0\xe0",
"\xda\xfa","\xd8\xf8","\xc3\xe3","\xd6\xf6","\xc2\xe2","\xce\xee","\xcd\xed","\xa7\xb7","\xa4\xb4","/?","/|","\x0\x0","\x0\x0"
};

/* Ukraine, Typewriter */
static const char main_key_ua_typewriter[MAIN_LEN][2] =
{
"'\"","!1","\xb0""2","/3",";4",":5",",6",".7","_8","?9","%0","-_","=+",
"\xca\xea","\xc3\xe3","\xd5\xf5","\xcb\xeb","\xc5\xe5","\xce\xee","\xc7\xe7","\xdb\xfb","\xdd\xfd","\xda\xfa","\xc8\xe8","\xad\xbd",
"\xc6\xe6","\xc9\xe9","\xd7\xf7","\xc1\xe1","\xd0\xf0","\xd2\xf2","\xcf\xef","\xcc\xec","\xc4\xe4","\xd6\xf6","\xa4\xb4","()",
"\xd1\xf1","\xde\xfe","\xd3\xf3","\xcd\xed","\xa6\xb6","\xd4\xf4","\xd8\xf8","\xc2\xe2","\xc0\xe0","\xa7\xb7","/|","\x0\x0","\x0\x0"
};

/* Ukraine, Winkeys */
static const char main_key_ua_winkeys[MAIN_LEN][2] =
{
"'~","1!","2\"","3\xb0","4;","5%","6:","7?","8*","9(","0)","-_","=+",
"\xca\xea","\xc3\xe3","\xd5\xf5","\xcb\xeb","\xc5\xe5","\xce\xee","\xc7\xe7","\xdb\xfb","\xdd\xfd","\xda\xfa","\xc8\xe8","\xa7\xb7",
"\xc6\xe6","\xa6\xb6","\xd7\xf7","\xc1\xe1","\xd0\xf0","\xd2\xf2","\xcf\xef","\xcc\xec","\xc4\xe4","\xd6\xf6","\xa4\xb4","\xad\xbd",
"\xd1\xf1","\xde\xfe","\xd3\xf3","\xcd\xed","\xc9\xe9","\xd4\xf4","\xd8\xf8","\xc2\xe2","\xc0\xe0",".,","/|","\x0\x0","\x0\x0"
};

/* Ukraine, Standard RSTU */
static const char main_key_ua_rstu[MAIN_LEN][2] =
{
"'?","!1","\"2","#3",";4",":5",",6",".7","*8","(9",")0","-_","=+",
"\xca\xea","\xc3\xe3","\xd5\xf5","\xcb\xeb","\xc5\xe5","\xce\xee","\xc7\xe7","\xdb\xfb","\xdd\xfd","\xda\xfa","\xc8\xe8","\xad\xbd",
"\xc6\xe6","\xc9\xe9","\xd7\xf7","\xc1\xe1","\xd0\xf0","\xd2\xf2","\xcf\xef","\xcc\xec","\xc4\xe4","\xd6\xf6","\xa4\xb4","/%",
"\xd1\xf1","\xde\xfe","\xd3\xf3","\xcd\xed","\xa6\xb6","\xd4\xf4","\xd8\xf8","\xc2\xe2","\xc0\xe0","\xa7\xb7","/|","\x0\x0","\x0\x0"
};

/* Ukraine, Standard RSTU on Russian layout */
static const char main_key_ua_rstu_ru[MAIN_LEN][2] =
{
"'?","!1","\"2","#3",";4",":5",",6",".7","*8","(9",")0","-_","=+",
"\xca\xea","\xc3\xe3","\xd5\xf5","\xcb\xeb","\xc5\xe5","\xce\xee","\xc7\xe7","\xdb\xfb","\xdd\xfd","\xda\xfa","\xc8\xe8","\xdf\xff",
"\xc6\xe6","\xd9\xf9","\xd7\xf7","\xc1\xe1","\xd0\xf0","\xd2\xf2","\xcf\xef","\xcc\xec","\xc4\xe4","\xd6\xf6","\xdc\xfc","/%",
"\xd1\xf1","\xde\xfe","\xd3\xf3","\xcd\xed","\xc9\xe9","\xd4\xf4","\xd8\xf8","\xc2\xe2","\xc0\xe0","\xa3\xb3","/|","\x0\x0","\x0\x0"
};

/* United Kingdom */
static const char main_key_gb[MAIN_LEN][2] =
{
"`\xac","1!","2\"","3\xa3","4$","5%","6^","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","[{","]}",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL",";:","'@","#~",
"zZ","xX","cC","vV","bB","nN","mM",",<",".>","/?","\\|","\x0\x0","\x0\x0"
};

/* United Kingdom, International (with dead keys) */
static const char main_key_gb_intl[MAIN_LEN][2] =
{
"P\xac","1!","2W","3\xa3","4$","5%","6R","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","[{","]}",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL",";:","Q@","#S",
"zZ","xX","cC","vV","bB","nN","mM",",<",".>","/?","\\|","\x0\x0","\x0\x0"
};

/* United Kingdom, Dvorak */
static const char main_key_gb_dvorak[MAIN_LEN][2] =
{
"`~","1!","2\"","3\xa3","4$","5%","6^","7&","8*","9(","0)","[{","]}",
"'@",",<",".>","pP","yY","fF","gG","cC","rR","lL","/?","=+",
"aA","oO","eE","uU","iI","dD","hH","tT","nN","sS","-_","#~",
";:","qQ","jJ","kK","xX","bB","mM","wW","vV","zZ","\\|","\x0\x0","\x0\x0"
};

/* United Kingdom, Macintosh */
static const char main_key_gb_mac[MAIN_LEN][2] =
{
"`~","1!","2@","3\xa3","4$","5%","6^","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","[{","]}",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL",";:","'\"","\\|",
"zZ","xX","cC","vV","bB","nN","mM",",<",".>","/?","<>","\x0\x0","\x0\x0"
};

/* Uzbekistan */
static const char main_key_uz[MAIN_LEN][2] =
{
"\xa3\xb3","1!","2\"","3#","4;","5%","6:","7?","8*","9(","0)","\xae\xbe","\x9b\x9a",
"\xca\xea","\xc3\xe3","\xd5\xf5","\xcb\xeb","\xc5\xe5","\xce\xee","\xc7\xe7","\xdb\xfb","\xc8\xe8","\xdf\xff","\x93\x92","\xb3\xb2",
"\xc6\xe6","\xda\xfa","\xd7\xf7","\xc1\xe1","\xd0\xf0","\xd2\xf2","\xcf\xef","\xcc\xec","\xc4\xe4","\xd6\xf6","\xdc\xfc","\\|",
"\xd1\xf1","\xde\xfe","\xd3\xf3","\xcd\xed","\xc9\xe9","\xd4\xf4","\xd8\xf8","\xc2\xe2","\xc0\xe0",".,","/|","\x0\x0","\x0\x0"
};

/* Vietnam */
static const char main_key_vn[MAIN_LEN][2] =
{
"`~","\xe3\xc3","\xe2\xc2","\xea\xca","\xf4\xd4","P%","a^","S&","Q*","`(","\xf0\xd0","-_","\xab+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xb0\xaf","\xa1\xa0",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL",";:","'\"","\\|",
"zZ","xX","cC","vV","bB","nN","mM",",<",".>","/?","<>","\x0\x0","\x0\x0"
};

/* Japan (PC-98xx Series) */
static const char main_key_nec_vndr_jp[MAIN_LEN][2] =
{
"\x0\x0","1!","2\"","3#","4$","5%","6&","7'","8(","9)","00","-=","^`",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","@~","[{",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL",";+",":*","\\|",
"zZ","xX","cC","vV","bB","nN","mM",",<",".>","/?","<>","\x0_","\x0\x0"
};

/* Ireland, Ogham */
static const char main_key_ie_ogam[MAIN_LEN][2] =
{
"\x9c\x9c","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0","\x0\x0",
"\x8a\x8a","\x95\x95","\x93\x93","\x8f\x8f","\x88\x88","\x98\x98","\x92\x92","\x94\x94","\x91\x91","\x9a\x9a","\x0\x0","\x0\x0",
"\x90\x90","\x84\x84","\x87\x87","\x83\x83","\x8c\x8c","\x86\x86","\x97\x97","\x96\x96","\x82\x82","\x0\x0","\x0\x0","\x80\x80",
"\x8e\x8e","\x99\x99","\x89\x89","\x8d\x8d","\x81\x81","\x85\x85","\x8b\x8b","\x9c\x9c","\x9b\x9b","\x80\x80","\x9b\x9c","\x0\x0","\x0\x0"
};

/* Ireland, Ogham IS434 */
static const char main_key_ie_ogam_is434[MAIN_LEN][2] =
{
"`\xac","1!","2\"","3\xa3","4$","5%","6^","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","\x0\x0","\x0\x0",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\x0\x0","\x0\x0","#~",
"zZ","xX","cC","vV","bB","nN","mM",",<",".>","/?","\\|","\x0\x0","\x0\x0"
};

/* Maldives */
static const char main_key_mv[MAIN_LEN][2] =
{
"`~","1!","2@","3#","4$","5%","6^","7&","8*","9(","0)","-_","=+",
"\xb0\xa4","\x87\xa2","\xac\xad","\x83\x9c","\x8c\x93","\x94\xa0","\xaa\xab","\xa8\xa9","\xae\xaf","\x95\xf7","[{","]}",
"\xa6\xa7","\x90\x81","\x8b\x91","\x8a\xf2","\x8e\xa3","\x80\x99","\x96\x9b","\x86\x9a","\x8d\x85",";:","'\"","\\|",
"\x92\xa1","\xd7\x98","\x97\x9d","\x88\xa5","\x84\x9e","\x82\x8f","\x89\x9f","\xac<",".>","/\xbf","|\xa6","\x0\x0","\x0\x0"
};

/* Esperanto */
static const char main_key_epo[MAIN_LEN][2] =
{
"`~","1!","2\"","3#","4$","5%","6'","7&","8*","9(","0)","-_","=+",
"\xfe\xde","\xbc\xac","eE","rR","tT","\xf8\xd8","uU","iI","oO","pP","[{","]}",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xfd\xdd","\xb6\xa6","\\|",
"zZ","\xe6\xc6","cC","vV","bB","nN","mM",",;",".:","/?","<>","\x0\x0","\x0\x0"
};

/* Nepal */
static const char main_key_np[MAIN_LEN][2] =
{
"=<","gg","hh","ii","jj","kk","ll","mm","nn","oo","fp","-R","=\xc",
"\x1f\x20","L\x14","GH","0C","$%","/\x1e","AB","?@","K\x13","*+","\x7\x8","\xf\x10",
">\x6","86","&'","\x9\xa","\x17\x18","9\x5","\x1c\x1d","\x15\x16","23",";:","'\"","P\x3",
"7\xb","!\"","\x1b\x1a","5\x1",",-","(#",".\x2",",\x19","de","M?","<>","\x0\x0","\x0\x0"
};

/* Nigeria */
static const char main_key_ng[MAIN_LEN][2] =
{
"`~","1!","2@","3#","4\xa6","5%","6^","7&","8*","9(","0)","-_","=+",
"qQ","wW","eE","rR","tT","yY","uU","iI","oO","pP","[{","]}",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL",";:","'\"","\\|",
"zZ","xX","cC","vV","bB","nN","mM",",<",".>","/?","<>","\x0\x0","\x0\x0"
};

/* Nigeria, Igbo */
static const char main_key_ng_igbo[MAIN_LEN][2] =
{
"`~","1!","2@","3#","4\xa6","5%","6^","7&","8*","9(","0)","-_","=+",
"\xcb\xca","wW","eE","rR","tT","yY","uU","iI","oO","pP","\xcd\xcc","]}",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL","\xe5\xe4","'\"","\\|",
"zZ","ED","cC","vV","bB","nN","mM",",<",".>","/?","<>","\x0\x0","\x0\x0"
};

/* Nigeria, Yoruba */
static const char main_key_ng_yoruba[MAIN_LEN][2] =
{
"`~","1!","2@","3#","4\xa6","5%","6^","7&","8*","9(","0)","-_","=+",
"\xb9\xb8","wW","eE","rR","tT","yY","uU","iI","oO","pP","[{","]}",
"aA","sS","dD","fF","gG","hH","jJ","kK","lL",";:","'\"","\\|",
"zZ","\xcd\xcc","cC","cb","bB","nN","mM",",<",".>","/?","<>","\x0\x0","\x0\x0"
};

/* Nigeria, Hausa */
static const char main_key_ng_hausa[MAIN_LEN][2] =
{
"`~","1!","2@","3#","4\xa6","5%","6^","7&","8*","9(","0)","-_","=+",
"qQ","\xfc\xdc","eE","rR","tT","yY","uU","iI","oO","pP","\xb4\xb3","\xe7\xe6",
"aA","sS","W\x8a","fF","gG","hH","jJ","kK","lL","10","\xdd\x8e","\\|",
"r\x9d","xX","cC","vV","bB","nN","\xf1\xd1",",<","\xe7\xc7","\xba\xaa","<>","\x0\x0","\x0\x0"
};

