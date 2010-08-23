# Copyright (C) 2006-2010 Oracle Corporation
#
# Oracle Corporation confidential
# All rights reserved
#
# drop all lines not including a src property.
/src=\"/!d
# extract the file spec
s/^.*src="\([^"]*\).*$/\1 /
# convert to unix slashes
s/\\/\//g
# $(env.PATH_OUT stuff.
s/(env\./(/g
# pretty
s/$/\\/
s/^/\t/

