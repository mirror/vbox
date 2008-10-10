# $Id$
## @file
#
# Just some quit sed hacks for validating an op.S assembly file.
# Will try this with gcc 4.x later to see if we can permit gcc 4
# to build op.c by using this script as guard against bad code.
#

## @todo need to check that we've the got two __op_label[0-1].op_goto_tb[0-1] symbols!

# if (ret) goto return
/^[[:space:]]*ret[[:space:]]*$/b return
#/^[[:space:]]*retn[[:space:]]*$/b bad

# if (jmp) goto jump
/^[[:space:]]*[^j]/b skip_jump_checks
/^[[:space:]]*jmp[[:space:]][[:space:]]*/b jump
/^[[:space:]]*ja[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jae[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jb[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jbe[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jc[[:space:]][[:space:]]*/b jump
/^[[:space:]]*je[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jg[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jge[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jl[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jle[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jnae[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jnb[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jnbe[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jnc[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jne[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jng[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jnge[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jnl[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jnle[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jno[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jnp[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jns[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jnz[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jo[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jp[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jpe[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jpo[[:space:]][[:space:]]*/b jump
/^[[:space:]]*js[[:space:]][[:space:]]*/b jump
/^[[:space:]]*jz[[:space:]][[:space:]]*/b jump
:skip_jump_checks

# Everything else is discarded!
d
b end


#
# Verify that all ret statements are at the end of a function by
# inspecting what's on the following line. It must either be a
# .size statement, a .LfeXXXX label, a .LfeXXXX label or #NO_APP comment.
#
# @todo figure out how to discard the first line in a simpler fashion.
:return
N
s/^[[:blank:]]*ret[[:blank:]]*\n*[[:blank:]]*//
/\.Lfe[0-9][0-9]*:/d
/\.LFE[0-9][0-9]*:/d
/size[[:space:]]/d
/^[/#]NO_APP[[:space:]]*$/d
/^$/!b bad
b end

#
# Verify that all jumps are to internal labels or to few select
# external labels.
#
#/^[[:blank:]]*jmp/
:jump
s/^[[:space:]]*j[a-z]*[[:space:]][[:space:]]*//
/^\.L/d
/^[1-9][fb]$/d
/^__op_gen_label1$/d
# two very special cases.
/^\*__op_param1+48[[:space:]][[:space:]]*#[[:space:]]*<variable>.tb_next[[:space:]]*$/d
/^\*__op_param1+52[[:space:]][[:space:]]*#[[:space:]]*<variable>.tb_next[[:space:]]*$/d
/^$/!b bad
b end

# An error was found
:bad
q 1

# next expression
:end

