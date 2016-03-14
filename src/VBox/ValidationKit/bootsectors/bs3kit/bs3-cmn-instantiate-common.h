

/*
 * Instantiating common code (c16, c32, c64).
 */
#define BS3_INSTANTIATING_CMN

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

#undef BS3_INSTANTIATING_CMN


/*
 * Instantiating code for each individual mode (rm, pe16, pe16_32, ..).
 */
#define BS3_INSTANTIATING_MODE

#ifdef BS3_MODE_INSTANTIATE_FILE1

# define BS3_MODE_INSTANTIATE_FILE1_B <BS3_MODE_INSTANTIATE_FILE1>

# if ARCH_BITS == 16 /* 16-bit */

#  define TMPL_MODE BS3_MODE_RM
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_MODE_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>

#  define TMPL_MODE BS3_MODE_PE16
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_MODE_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>

#  define TMPL_MODE BS3_MODE_PE16_V86
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_MODE_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>

#  define TMPL_MODE BS3_MODE_PE32_16
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_MODE_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>

#  define TMPL_MODE BS3_MODE_PEV86
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_MODE_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>

#  define TMPL_MODE BS3_MODE_PP16
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_MODE_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>

#  define TMPL_MODE BS3_MODE_PP16_V86
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_MODE_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>

#  define TMPL_MODE BS3_MODE_PP32_16
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_MODE_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>

#  define TMPL_MODE BS3_MODE_PPV86
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_MODE_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>

#  define TMPL_MODE BS3_MODE_PAE16
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_MODE_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>

#  define TMPL_MODE BS3_MODE_PAE16_V86
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_MODE_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>

#  define TMPL_MODE BS3_MODE_PAE32_16
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_MODE_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>

#  define TMPL_MODE BS3_MODE_PAEV86
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_MODE_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>

#  define TMPL_MODE BS3_MODE_LM16
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_MODE_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>

# endif

# if ARCH_BITS == 32 /* 32-bit  */

#  define TMPL_MODE BS3_MODE_PE16_32
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_MODE_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>

#  define TMPL_MODE BS3_MODE_PE32
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_MODE_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>

#  define TMPL_MODE BS3_MODE_PP16_32
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_MODE_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>

#  define TMPL_MODE BS3_MODE_PP32
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_MODE_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>

#  define TMPL_MODE BS3_MODE_PAE16_32
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_MODE_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>

#  define TMPL_MODE BS3_MODE_PAE32
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_MODE_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>

#  define TMPL_MODE BS3_MODE_LM32
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_MODE_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>

# endif

# if ARCH_BITS == 64 /* 64-bit. */
#  define TMPL_MODE BS3_MODE_LM64
#  include <bs3kit/bs3kit-template-header.h>
#  include BS3_MODE_INSTANTIATE_FILE1_B
#  include <bs3kit/bs3kit-template-footer.h>
# endif

#endif

