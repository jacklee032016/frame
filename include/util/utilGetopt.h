/* 
 *
 */

#ifndef __BASE_GETOPT_H__
#define __BASE_GETOPT_H__ 1

/**
 * @brief Compile time settings
 */

/**
 * @defgroup UTIL_GETOPT Getopt
 * @ingroup TEXT
 * @{
 */

#ifdef	__cplusplus
extern "C" {
#endif

/* For communication from `bgetopt' to the caller.
   When `bgetopt' finds an option that takes an argument,
   the argument value is returned here.
   Also, when `ordering' is RETURN_IN_ORDER,
   each non-option ARGV-element is returned here.  */

extern char *boptarg;

/* Index in ARGV of the next element to be scanned.
   This is used for communication to and from the caller
   and for communication between successive calls to `bgetopt'.

   On entry to `bgetopt', zero means this is the first call; initialize.

   When `bgetopt' returns -1, this is the index of the first of the
   non-option elements that the caller should itself scan.

   Otherwise, `boptind' communicates from one call to the next
   how much of ARGV has been scanned so far.  */

extern int boptind;

/* Set to an option character which was unrecognized.  */

extern int boptopt;

/* Describe the long-named options requested by the application.
   The LONG_OPTIONS argument to bgetopt_long or bgetopt_long_only is a vector
   of `struct bgetopt_option' terminated by an element containing a name which is
   zero.

   The field `has_arg' is:
   no_argument		(or 0) if the option does not take an argument,
   required_argument	(or 1) if the option requires an argument,
   optional_argument 	(or 2) if the option takes an optional argument.

   If the field `flag' is not NULL, it points to a variable that is set
   to the value given in the field `val' when the option is found, but
   left unchanged if the option is not found.

   To have a long-named option do something other than set an `int' to
   a compiled-in constant, such as set a value from `boptarg', set the
   option's `flag' field to zero and its `val' field to a nonzero
   value (the equivalent single-letter option character, if there is
   one).  For long options that have a zero `flag' field, `bgetopt'
   returns the contents of the `val' field.  */

struct bgetopt_option
{
  const char *name;
  /* has_arg can't be an enum because some compilers complain about
     type mismatches in all the code that assumes it is an int.  */
  int has_arg;
  int *flag;
  int val;
};

/* Names for the values of the `has_arg' field of `struct bgetopt_option'.  */

# define no_argument		0
# define required_argument	1
# define optional_argument	2


/* Get definitions and prototypes for functions to process the
   arguments in ARGV (ARGC of them, minus the program name) for
   options given in OPTS.

   Return the option character from OPTS just read.  Return -1 when
   there are no more options.  For unrecognized options, or options
   missing arguments, `boptopt' is set to the option letter, and '?' is
   returned.

   The OPTS string is a list of characters which are recognized option
   letters, optionally followed by colons, specifying that that letter
   takes an argument, to be placed in `boptarg'.

   If a letter in OPTS is followed by two colons, its argument is
   optional.  This behavior is specific to the GNU `bgetopt'.

   The argument `--' causes premature termination of argument
   scanning, explicitly telling `bgetopt' that there are no more
   options.

   If OPTS begins with `--', then non-option arguments are treated as
   arguments to the option '\0'.  This behavior is specific to the GNU
   `bgetopt'.  */

int bgetopt (int argc, char *const *argv, const char *shortopts);

int bgetopt_long (int argc, char *const *argv, const char *options,
		        const struct bgetopt_option *longopts, int *longind);
int bgetopt_long_only (int argc, char *const *argv,
			     const char *shortopts,
		             const struct bgetopt_option *longopts, int *longind);


#ifdef	__cplusplus
}
#endif

#endif

