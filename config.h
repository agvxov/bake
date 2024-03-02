/* Require space after START */
#define REQUIRE_SPACE

/* colorize output */
#define ENABLE_COLOR 1

/* preferred,     @FILENAME @SHORT @ARGS */
#define NEW_MACROS 1
/*                $@        $*     $+    */
#define OLD_MACROS 1

/* ./bake bake will compile bake.c, basically just proves that binary files really are supported,
 * the bake.c file must exist next to the executable for this work correctly. Not meant as a serious feature,
 * DO NOT enable this by default or in user builds. */
#define INCLUDE_AUTONOMOUS_COMPILE 0

#if ENABLE_COLOR == 1
# define    RED "\033[91m"
# define  GREEN "\033[92m"
# define YELLOW "\033[93m"
# define    DIM "\033[2m"
# define   BOLD "\033[1m"
# define  RESET "\033[0m"
#elif ENABLE_COLOR
# define    RED "\033[91;5m"
# define  GREEN "\033[96;7m"
# define YELLOW "\033[94m"
# define    DIM "\033[7m"
# define   BOLD "\033[1;4m"
# define  RESET "\033[0m"
#else
# define    RED
# define  GREEN
# define YELLOW
# define    DIM
# define   BOLD
# define  RESET
#endif
