#
# Strip stuff lines and spaces we don't care about.
#
/ %line /d
/\[section /d
/\[bits /d
/\[absolute /d
/ times /d
s/ *[[:digit:]]* //
/^ *$/d
s/ *$//g
s/^ *//g
/^\.text$/d
/^\.data$/d
/^\.bss$/d

#
# Figure which type of line this is and process it accordingly.
#
/^[[:alpha:]_][[:alnum:]_]*:/b struct
/<gap>/b member
/^\.[[:alpha:]_][[:alnum:]_.:]* res.*$/b member_two
b error
b member_two


#
# Struct start / end.
#
:struct
s/:$//
h
s/^/global /
s/$/ ; struct/
b end


#
# Struct member
# Note: the 't' command doesn't seem to be working right with 's'.
#
:member
s/[[:xdigit:]]* *//
s/<gap> *//
/^\.[[:alnum:]_.]*[:]* .*$/!t error
s/\(\.[[:alnum:]_]*\)[:]* .*$/\1 /
G
s/^\([^ ]*\) \(.*\)$/global \2\1 ; member/
s/\n//m

b end


#
# Struct member, no address. yasm r1842 and later.
#
:member_two
s/[:]*  *res[bwdtq] .*$//
s/$/ /
/^\.[[:alnum:]_.]* *$/!t error
G
s/^\([^ ]*\) \(.*\)$/global \2\1 ; member2/
s/\n//m

b end


:error
s/^/\nSed script logic error!\nBuffer: /
s/$/\nHold: /
G
q 1
b end


:end

