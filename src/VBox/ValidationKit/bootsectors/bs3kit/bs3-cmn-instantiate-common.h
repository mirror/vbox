


#ifdef BS3_CMN_INSTANTIATE_FILE1

# define BS3_CMN_INSTANTIATE_FILE1_B <BS3_CMN_INSTANTIATE_FILE1>

# if ARCH_BITS == 16 /* 16-bit - real mode. */
#  define TMPL_MODE BS3_MODE_RM
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_CMN_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>
# endif

# if ARCH_BITS == 32 /* 32-bit - paged protected mode. */
#  define TMPL_MODE BS3_MODE_PP32
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_CMN_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>
# endif

# if ARCH_BITS == 64 /* 64-bit. */
#  define TMPL_MODE BS3_MODE_LM64
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_CMN_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>
# endif

#endif

