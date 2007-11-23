/* GNU m4 -- A simple macro processor

   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 1999, 2000, 2003,
   2004, 2005, 2006, 2007 Free Software Foundation, Inc.

   This file is part of GNU M4.

   GNU M4 is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   GNU M4 is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <config.h>

#include <locale.h>

#include "m4.h"

#include "close-stream.h"
#include "closein.h"
#include "configmake.h"
#include "getopt.h"
#include "version-etc.h"
#include "xstrtol.h"

#define AUTHORS _("Rene' Seindal"), "Gary V. Vaughan", "Eric Blake"

typedef struct deferred
{
  struct deferred *next;
  int code;			/* deferred optchar */
  const char *value;
} deferred;


/* Error handling functions.  */

#ifdef USE_STACKOVF

/* Tell user stack overflowed and abort.  */
static void
stackovf_handler (void)
{
  /* FIXME - calling gettext and error inside a signal handler is dangerous,
     since these functions invoke functions that are not signal-safe.  We
     are sort of justified by the fact that we will exit and never return,
     but this should really be fixed.  */
  error (EXIT_FAILURE, 0, _("stack overflow (infinite define recursion?)"));
}

#endif /* USE_STACKOV */



/* Print a usage message and exit with STATUS.  */
static void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    xfprintf (stderr, _("Try `%s --help' for more information.\n"),
	      m4_get_program_name ());
  else
    {
      xprintf (_("Usage: %s [OPTION]... [FILE]...\n"), m4_get_program_name ());
      fputs (_("\
Process macros in FILEs.\n\
If no FILE or if FILE is `-', standard input is read.  If no FILE, and both\n\
standard input and standard output are terminals, -i is implied.\n\
"), stdout);
      puts ("");
      fputs (_("\
Mandatory or optional arguments to long options are mandatory or optional\n\
for short options too.\n\
\n\
Operation modes:\n\
      --help                   display this help and exit\n\
      --version                output version information and exit\n\
"), stdout);
      fputs (_("\
  -b, --batch                  buffer output, process interrupts\n\
  -c, --discard-comments       do not copy comments to the output\n\
  -E, --fatal-warnings         once: warnings become errors, twice: stop\n\
                               execution at first error\n\
  -i, --interactive            unbuffer output, ignore interrupts\n\
  -P, --prefix-builtins        force a `m4_' prefix to all builtins\n\
  -Q, --quiet, --silent        suppress some warnings for builtins\n\
  -r, --regexp-syntax[=SPEC]   set default regexp syntax to SPEC [GNU_M4]\n\
      --safer                  disable potentially unsafe builtins\n\
  -W, --warnings               enable all warnings\n\
"), stdout);
      puts ("");
      fputs (_("\
SPEC is any one of:\n\
  AWK, BASIC, BSD_M4, ED, EMACS, EXTENDED, GNU_AWK, GNU_EGREP, GNU_M4,\n\
  GREP, POSIX_AWK, POSIX_EGREP, MINIMAL, MINIMAL_BASIC, SED.\n\
"), stdout);
      puts ("");
      xprintf (_("\
Dynamic loading features:\n\
  -M, --module-directory=DIR   add DIR to module search path before\n\
                               `%s'\n\
  -m, --load-module=MODULE     load dynamic MODULE\n\
      --unload-module=MODULE   unload dynamic MODULE\n\
"), PKGLIBEXECDIR);
      puts ("");
      fputs (_("\
Preprocessor features:\n\
  -B, --prepend-include=DIR    add DIR to include path before `.'\n\
  -D, --define=NAME[=VALUE]    define NAME as having VALUE, or empty\n\
      --import-environment     import all environment variables as macros\n\
  -I, --include=DIR            add DIR to include path after `.'\n\
"), stdout);
      fputs (_("\
      --popdef=NAME            popdef NAME\n\
  -p, --pushdef=NAME[=VALUE]   pushdef NAME as having VALUE, or empty\n\
  -s, --synclines              short for --syncoutput=1\n\
      --syncoutput[=STATE]     set generation of `#line NUM \"FILE\"' lines\n\
                               to STATE (0=off, 1=on, default 0 if omitted)\n\
  -U, --undefine=NAME          undefine NAME\n\
"), stdout);
      puts ("");
      fputs (_("\
Limits control:\n\
  -g, --gnu                    override -G to re-enable GNU extensions\n\
  -G, --traditional, --posix   suppress all GNU extensions\n\
  -L, --nesting-limit=NUMBER   change artificial nesting limit [1024]\n\
"), stdout);
      puts ("");
      fputs (_("\
Frozen state files:\n\
  -F, --freeze-state=FILE      produce a frozen state on FILE at end\n\
  -R, --reload-state=FILE      reload a frozen state from FILE at start\n\
"), stdout);
      puts ("");
      fputs (_("\
Debugging:\n\
  -d, --debug[=FLAGS], --debugmode[=FLAGS]\n\
                               set debug level (no FLAGS implies `aeq')\n\
      --debugfile=FILE         redirect debug and trace output\n\
  -l, --debuglen=NUM           restrict macro tracing size\n\
  -t, --trace=NAME, --traceon=NAME\n\
                               trace NAME when it is defined\n\
      --traceoff=NAME          no longer trace NAME\n\
"), stdout);
      puts ("");
      fputs (_("\
FLAGS is any of:\n\
  a   show actual arguments in trace\n\
  c   show definition line in trace\n\
  e   show expansion in trace\n\
  f   include current input file name in trace and debug\n\
  i   show changes in input files in debug\n\
  l   include current input line number in trace and debug\n\
"), stdout);
      fputs (_("\
  m   show module information in trace, debug, and dumpdef\n\
  p   show results of path searches in debug\n\
  q   quote values as necessary in dumpdef and trace, useful with a or e\n\
  s   show full stack of pushdef values in dumpdef\n\
  t   trace all macro calls, regardless of named traceon state\n\
  x   include unique macro call id in trace, useful with c\n\
  V   shorthand for all of the above flags\n\
"), stdout);
      puts ("");
      fputs (_("\
If defined, the environment variable `M4PATH' is a colon-separated list\n\
of directories included after any specified by `-I', and the variable\n\
`M4MODPATH' is a colon-separated list of directories searched before any\n\
specified by `-M'.  The environment variable `POSIXLY_CORRECT' implies\n\
-G -Q; otherwise GNU extensions are enabled by default.\n\
"), stdout);
      puts ("");
      fputs (_("\
Exit status is 0 for success, 1 for failure, 63 for frozen file version\n\
mismatch, or whatever value was passed to the m4exit macro.\n\
"), stdout);
      puts ("");
      /* TRANSLATORS: the placeholder indicates the bug-reporting
	 address for this application.  Please add _another line_
	 saying "Report translation bugs to <...>\n" with the address
	 for translation bugs (typically your translation team's web
	 or email address).  */
      xprintf (_("Report bugs to <%s>.\n"), PACKAGE_BUGREPORT);
    }
  exit (status);
}

/* For long options that have no equivalent short option, use a
   non-character as a pseudo short option, starting with CHAR_MAX + 1.  */
enum
{
  ARGLENGTH_OPTION = CHAR_MAX + 1,	/* not quite -l, because of message */
  DEBUGFILE_OPTION,			/* no short opt */
  DIVERSIONS_OPTION,			/* not quite -N, because of message */
  ERROR_OUTPUT_OPTION,			/* not quite -o, because of message */
  HASHSIZE_OPTION,			/* not quite -H, because of message */
  IMPORT_ENVIRONMENT_OPTION,		/* no short opt */
  POPDEF_OPTION,			/* no short opt */
  PREPEND_INCLUDE_OPTION,		/* not quite -B, because of message */
  SAFER_OPTION,				/* -S still has old no-op semantics */
  SYNCOUTPUT_OPTION,			/* not quite -s, because of opt arg */
  TRACEOFF_OPTION,			/* no short opt */
  UNLOAD_MODULE_OPTION,			/* no short opt */
  WORD_REGEXP_OPTION,			/* deprecated, used to be -W */

  HELP_OPTION,				/* no short opt */
  VERSION_OPTION			/* no short opt */
};

/* Decode options and launch execution.  */
static const struct option long_options[] =
{
  {"batch", no_argument, NULL, 'b'},
  {"debug", optional_argument, NULL, 'd'},
  {"debuglen", required_argument, NULL, 'l'},
  {"debugmode", optional_argument, NULL, 'd'},
  {"define", required_argument, NULL, 'D'},
  {"discard-comments", no_argument, NULL, 'c'},
  {"fatal-warnings", no_argument, NULL, 'E'},
  {"freeze-state", required_argument, NULL, 'F'},
  {"gnu", no_argument, NULL, 'g'},
  {"include", required_argument, NULL, 'I'},
  {"interactive", no_argument, NULL, 'i'},
  {"load-module", required_argument, NULL, 'm'},
  {"module-directory", required_argument, NULL, 'M'},
  {"nesting-limit", required_argument, NULL, 'L'},
  {"posix", no_argument, NULL, 'G'},
  {"prefix-builtins", no_argument, NULL, 'P'},
  {"pushdef", required_argument, NULL, 'p'},
  {"quiet", no_argument, NULL, 'Q'},
  {"regexp-syntax", optional_argument, NULL, 'r'},
  {"reload-state", required_argument, NULL, 'R'},
  {"silent", no_argument, NULL, 'Q'},
  {"synclines", no_argument, NULL, 's'},
  {"trace", required_argument, NULL, 't'},
  {"traceon", required_argument, NULL, 't'},
  {"traditional", no_argument, NULL, 'G'},
  {"undefine", required_argument, NULL, 'U'},
  {"warnings", no_argument, NULL, 'W'},

  {"arglength", required_argument, NULL, ARGLENGTH_OPTION},
  {"debugfile", required_argument, NULL, DEBUGFILE_OPTION},
  {"diversions", required_argument, NULL, DIVERSIONS_OPTION},
  {"hashsize", required_argument, NULL, HASHSIZE_OPTION},
  {"error-output", required_argument, NULL, ERROR_OUTPUT_OPTION},
  {"import-environment", no_argument, NULL, IMPORT_ENVIRONMENT_OPTION},
  {"popdef", required_argument, NULL, POPDEF_OPTION},
  {"prepend-include", required_argument, NULL, PREPEND_INCLUDE_OPTION},
  {"safer", no_argument, NULL, SAFER_OPTION},
  {"syncoutput", optional_argument, NULL, SYNCOUTPUT_OPTION},
  {"traceoff", required_argument, NULL, TRACEOFF_OPTION},
  {"unload-module", required_argument, NULL, UNLOAD_MODULE_OPTION},
  {"word-regexp", required_argument, NULL, WORD_REGEXP_OPTION},

  {"help", no_argument, NULL, HELP_OPTION},
  {"version", no_argument, NULL, VERSION_OPTION},

  { NULL, 0, NULL, 0 },
};

/* POSIX requires only -D, -U, and -s; and says that the first two
   must be recognized when interspersed with file names.  Traditional
   behavior also handles -s between files.  Starting OPTSTRING with
   '-' forces getopt_long to hand back file names as arguments to opt
   '\1', rather than reordering the command line.  */
#define OPTSTRING "-B:D:EF:GH:I:L:M:N:PQR:S:T:U:Wbcd::egil:m:o:p:r::st:"

/* For determining whether to be interactive.  */
enum interactive_choice
{
  INTERACTIVE_UNKNOWN,	/* Still processing arguments, no -b or -i yet */
  INTERACTIVE_YES,	/* -i specified last */
  INTERACTIVE_NO	/* -b specified last */
};

/* Convert OPT to size_t, reporting an error using long option index
   OI or short option character OPTCHAR if it does not fit.  */
static size_t
size_opt (char const *opt, int oi, int optchar)
{
  unsigned long int size;
  strtol_error status = xstrtoul (opt, NULL, 10, &size, "kKmMgGtTPEZY0");
  if (SIZE_MAX < size && status == LONGINT_OK)
    status = LONGINT_OVERFLOW;
  if (status != LONGINT_OK)
    xstrtol_fatal (status, oi, optchar, long_options, opt);
  return size;
}

/* Process a command line file NAME, and return true only if it was
   stdin.  */
static void
process_file (m4 *context, const char *name)
{
  if (strcmp (name, "-") == 0)
    m4_push_file (context, stdin, "stdin", false);
  else
    {
      char *full_name;
      FILE *fp = m4_path_search (context, name, &full_name);
      if (fp == NULL)
	{
	  m4_error (context, 0, errno, NULL, _("cannot open file `%s'"), name);
	  return;
	}
      m4_push_file (context, fp, full_name, true);
      free (full_name);
    }
  m4_macro_expand_input (context);
}


/* Main entry point.  Parse arguments, load modules, then parse input.  */
int
main (int argc, char *const *argv, char *const *envp)
{
  deferred *head = NULL;	/* head of deferred argument list */
  deferred *tail = NULL;
  deferred *defn;
  size_t size;			/* for parsing numeric option arguments */

  bool import_environment = false; /* true to import environment */
  bool seen_file = false;
  const char *debugfile = NULL;
  const char *frozen_file_to_read = NULL;
  const char *frozen_file_to_write = NULL;
  enum interactive_choice interactive = INTERACTIVE_UNKNOWN;

  m4 *context;

  int exit_status;

  /* Initialize gnulib error module.  */
  m4_set_program_name (argv[0]);
  atexit (close_stdin);

  setlocale (LC_ALL, "");
#ifdef ENABLE_NLS
  textdomain (PACKAGE);
#endif

  LTDL_SET_PRELOADED_SYMBOLS ();

  context = m4_create ();

  m4__module_init (context);

#ifdef USE_STACKOVF
  setup_stackovf_trap (argv, envp, stackovf_handler);
#endif

  if (getenv ("POSIXLY_CORRECT"))
    {
      m4_set_posixly_correct_opt (context, true);
      m4_set_suppress_warnings_opt (context, true);
    }

  /* First, we decode the arguments, to size up tables and stuff.
     Avoid lasting side effects; for example 'm4 --debugfile=oops
     --help' must not create the file `oops'.  */
  while (1)
    {
      int oi = -1;
      int optchar = getopt_long (argc, (char **) argv, OPTSTRING,
				 long_options, &oi);
      if (optchar == -1)
	break;

      switch (optchar)
	{
	default:
	  usage (EXIT_FAILURE);

	case 'H':
	case HASHSIZE_OPTION:
	  /* -H was supported in 1.4.x, but is a no-op now.  FIXME -
	     remove support for -H after 2.0.  */
	  error (0, 0, _("Warning: `%s' is deprecated"),
		 optchar == 'H' ? "-H" : "--hashsize");
	  break;

	case 'N':
	case DIVERSIONS_OPTION:
	  /* -N became an obsolete no-op in 1.4.x.  FIXME - remove
	     support for -N after 2.0.	*/
	  error (0, 0, _("Warning: `%s' is deprecated"),
		 optchar == 'N' ? "-N" : "--diversions");
	  break;

	case 'S':
	case 'T':
	  /* Compatibility junk: options that other implementations
	     support, but which we ignore as no-ops and don't list in
	     --help.  */
	  error (0, 0, _("Warning: `-%c' is deprecated"),
		 optchar);
	  break;

	case WORD_REGEXP_OPTION:
	  /* Supported in 1.4.x as -W, but no longer present.  */
	  error (0, 0, _("Warning: `%s' is deprecated"), "--word-regexp");
	  break;

	case 's':
	  optchar = SYNCOUTPUT_OPTION;
	  optarg = "1";
	  /* fall through */
	case 'D':
	case 'U':
	case 'm':
	case 'p':
	case 'r':
	case 't':
	case '\1':
	case POPDEF_OPTION:
	case SYNCOUTPUT_OPTION:
	case TRACEOFF_OPTION:
	case UNLOAD_MODULE_OPTION:
	  /* Arguments that cannot be handled until later are accumulated.  */

	  defn = xmalloc (sizeof *defn);
	  defn->code = optchar;
	  defn->value = optarg;
	  defn->next = NULL;

	  if (head == NULL)
	    head = defn;
	  else
	    tail->next = defn;
	  tail = defn;

	  if (optchar == '\1')
	    seen_file = true;

	  break;

	case 'B':
	  /* In 1.4.x, -B<num> was a no-op option for compatibility with
	     Solaris m4.  Warn if optarg is all numeric.  FIXME -
	     silence this warning after 2.0.  */
	  if (isdigit (to_uchar (*optarg)))
	    {
	      char *end;
	      errno = 0;
	      strtol (optarg, &end, 10);
	      if (*end == '\0' && errno == 0)
		error (0, 0, _("Warning: recommend using `-B ./%s' instead"),
		       optarg);
	    }
	  /* fall through */
	case PREPEND_INCLUDE_OPTION:
	  m4_add_include_directory (context, optarg, true);
	  break;

	case 'E':
	  if (m4_get_fatal_warnings_opt (context))
	    m4_set_warnings_exit_opt (context, true);
	  else
	    m4_set_fatal_warnings_opt (context, true);
	  break;

	case 'F':
	  frozen_file_to_write = optarg;
	  break;

	case 'G':
	  m4_set_posixly_correct_opt (context, true);
	  break;

	case 'I':
	  m4_add_include_directory (context, optarg, false);
	  break;

	case 'L':
	  size = size_opt (optarg, oi, optchar);
	  m4_set_nesting_limit_opt (context, size);
	  break;

	case 'M':
	  if (lt_dlinsertsearchdir (lt_dlgetsearchpath (), optarg) != 0)
	    {
	      const char *dlerr = lt_dlerror ();
	      if (dlerr == NULL)
		m4_error (context, EXIT_FAILURE, 0, NULL,
			  _("failed to add search directory `%s'"),
			  optarg);
	      else
		m4_error (context, EXIT_FAILURE, 0, NULL,
			  _("failed to add search directory `%s': %s"),
			  optarg, dlerr);
	    }
	  break;

	case 'P':
	  m4_set_prefix_builtins_opt (context, true);
	  break;

	case 'Q':
	  m4_set_suppress_warnings_opt (context, true);
	  break;

	case 'R':
	  frozen_file_to_read = optarg;
	  break;

	case 'W':
	  /* FIXME - should W take an optional argument, to allow -Wall,
	     -Wnone, -Werror, -Wcategory, -Wno-category?  If so, then have
	     -W == -Wall.  */
	  m4_set_suppress_warnings_opt (context, false);
	  break;

	case 'b':
	  interactive = INTERACTIVE_NO;
	  break;

	case 'c':
	  m4_set_discard_comments_opt (context, true);
	  break;

	case 'd':
	  {
	    int old = m4_get_debug_level_opt (context);
	    m4_set_debug_level_opt (context, m4_debug_decode (context, old,
							      optarg));
	  }
	  if (m4_get_debug_level_opt (context) < 0)
	    {
	      error (0, 0, _("bad debug flags: `%s'"), optarg);
	      m4_set_debug_level_opt (context, 0);
	    }
	  break;

	case 'e':
	  error (0, 0, _("Warning: `%s' is deprecated, use `%s' instead"),
		 "-e", "-i");
	  /* fall through */
	case 'i':
	  interactive = INTERACTIVE_YES;
	  break;

	case 'g':
	  m4_set_posixly_correct_opt (context, false);
	  break;

	case ARGLENGTH_OPTION:
	  error (0, 0, _("Warning: `%s' is deprecated, use `%s' instead"),
		 "--arglength", "--debuglen");
	  /* fall through */
	case 'l':
	  size = size_opt (optarg, oi, optchar);
	  m4_set_max_debug_arg_length_opt (context, size);
	  break;

	case 'o':
	case ERROR_OUTPUT_OPTION:
	  /* FIXME: -o is inconsistent with other tools' use of
	     -o/--output for creating an output file instead of using
	     stdout, and --error-output is misnamed since it does not
	     affect error messages to stderr.  Change the meaning of -o
	     after 2.1.	 */
	  error (0, 0, _("Warning: `%s' is deprecated, use `%s' instead"),
		 optchar == 'o' ? "-o" : "--error-output", "--debugfile");
	  /* fall through */
	case DEBUGFILE_OPTION:
	  /* Don't call m4_debug_set_output here, as it has side effects.  */
	  debugfile = optarg;
	  break;

	case IMPORT_ENVIRONMENT_OPTION:
	  import_environment = true;
	  break;

	case SAFER_OPTION:
	  m4_set_safer_opt (context, true);
	  break;

	case VERSION_OPTION:
	  version_etc (stdout, PACKAGE, PACKAGE_NAME TIMESTAMP,
		       VERSION, AUTHORS, NULL);
	  exit (EXIT_SUCCESS);
	  break;

	case HELP_OPTION:
	  usage (EXIT_SUCCESS);
	  break;
	}
    }

  /* Interactive if specified, or if no input files and stdin and
     stderr are terminals, to match sh behavior.  Interactive mode
     means unbuffered output, and interrupts ignored.  */

  m4_set_interactive_opt (context, (interactive == INTERACTIVE_YES
				    || (interactive == INTERACTIVE_UNKNOWN
					&& optind == argc && !seen_file
					&& isatty (STDIN_FILENO)
					&& isatty (STDERR_FILENO))));
  if (m4_get_interactive_opt (context))
    {
      signal (SIGINT, SIG_IGN);
      setbuf (stdout, NULL);
    }


  /* Do the basic initializations.  */
  if (debugfile && !m4_debug_set_output (context, debugfile))
    m4_error (context, 0, errno, NULL, _("cannot set debug file `%s'"),
	      debugfile);
  m4_input_init (context);
  m4_output_init (context);
  m4_include_env_init (context);

  if (frozen_file_to_read)
    reload_frozen_state (context, frozen_file_to_read);
  else
    {
      m4_module_load (context, "m4", NULL);
      if (m4_get_posixly_correct_opt (context))
	m4_module_load (context, "traditional", NULL);
      else
	m4_module_load (context, "gnu", NULL);
    }

  /* Import environment variables as macros.  The definition are
     prepended to the macro definition list, so -U can override
     environment variables. */

  if (import_environment)
    {
      char *const *env;

      for (env = envp; *env != NULL; env++)
	{
	  defn = xmalloc (sizeof *defn);
	  defn->code = 'D';
	  defn->value = *env;
	  defn->next = head;
	  head = defn;
	}
    }

  /* Handle deferred command line macro definitions.  Must come after
     initialization of the symbol table.  */
  defn = head;
  while (defn != NULL)
    {
      deferred *next;
      const char *arg = defn->value;

      switch (defn->code)
	{
	case 'D':
	case 'p':
	  {
	    m4_symbol_value *value = m4_symbol_value_create ();

	    /* defn->value is read-only, so we need a copy.  */
	    char *macro_name = xstrdup (arg);
	    char *macro_value = strchr (macro_name, '=');

	    if (macro_value != NULL)
	      *macro_value++ = '\0';
	    m4_set_symbol_value_text (value, xstrdup (macro_value
						      ? macro_value : ""));

	    if (defn->code == 'D')
	      m4_symbol_define (M4SYMTAB, macro_name, value);
	    else
	      m4_symbol_pushdef (M4SYMTAB, macro_name, value);
	    free (macro_name);
	  }
	  break;

	case 'U':
	  m4_symbol_delete (M4SYMTAB, arg);
	  break;

	case 'm':
	  /* FIXME - should loading a module result in output?  */
	  m4_module_load (context, arg, NULL);
	  break;

	case 'r':
	  m4_set_regexp_syntax_opt (context, m4_regexp_syntax_encode (arg));
	  if (m4_get_regexp_syntax_opt (context) < 0)
	    m4_error (context, EXIT_FAILURE, 0, NULL,
		      _("bad regexp syntax option: `%s'"), arg);
	  break;

	case 't':
	  m4_set_symbol_name_traced (M4SYMTAB, arg, true);
	  break;

	case '\1':
	  process_file (context, arg);
	  break;

	case POPDEF_OPTION:
	  if (m4_symbol_lookup (M4SYMTAB, arg))
	    m4_symbol_popdef (M4SYMTAB, arg);
	  break;

	case SYNCOUTPUT_OPTION:
	  {
	    bool previous = m4_get_syncoutput_opt (context);
	    m4_set_syncoutput_opt (context,
				   m4_parse_truth_arg (context, arg,
						       "--syncoutput",
						       previous));
	  }
	  break;

	case TRACEOFF_OPTION:
	  m4_set_symbol_name_traced (M4SYMTAB, arg, false);
	  break;

	case UNLOAD_MODULE_OPTION:
	  /* FIXME - should unloading a module result in output?  */
	  m4_module_unload (context, arg, NULL);
	  break;

	default:
	  assert (!"INTERNAL ERROR: bad code in deferred arguments");
	  abort ();
	}

      next = defn->next;
      free (defn);
      defn = next;
    }

  /* Handle remaining input files.  Each file is pushed on the input,
     and the input read.  */

  if (optind == argc && !seen_file)
    process_file (context, "-");
  else
    for (; optind < argc; optind++)
      process_file (context, argv[optind]);

  /* Now handle wrapup text.
     FIXME - when -F is in effect, should wrapped text be frozen?  */
  while (m4_pop_wrapup (context))
    m4_macro_expand_input (context);

  if (frozen_file_to_write)
    produce_frozen_state (context, frozen_file_to_write);
  else
    {
      m4_make_diversion (context, 0);
      m4_undivert_all (context);
    }

  /* The remaining cleanup functions systematically free all of the
     memory we still have pointers to.  By definition, if there is
     anything left when we're done: it was caused by a memory leak.
     Strictly, we don't need to do this, but it makes leak detection
     a whole lot easier!  */

  m4__module_exit (context);
  m4_output_exit ();
  m4_input_exit ();

  /* Change debug stream back to stderr, to force flushing the debug
     stream and detect any errors it might have encountered.  The
     three standard streams are closed by close_stdin.  */
  m4_debug_set_output (context, NULL);

  exit_status = m4_get_exit_status (context);
  m4_delete (context);

  m4_hash_exit ();

#ifdef USE_STACKOVF
  stackovf_exit ();
#endif

  exit (exit_status);
}
