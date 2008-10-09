# Extract all examples from the manual source.            -*- AWK -*-

# Copyright (C) 1992, 2000, 2001, 2006, 2007, 2008 Free Software
# Foundation, Inc.
#
# This file is part of GNU M4.
#
# GNU M4 is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# GNU M4 is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# This script is for use with any New AWK.

BEGIN {
  seq = -1;
  status = xfail = examples = 0;
  file = options = "";
  print "# This file is part of the GNU m4 test suite.  -*- Autotest -*-";
  # I don't know how to get this file's name, so it's hard coded :(
  print "# Do not edit by hand, it was generated by generate.awk.";
  print "#";
  print "# Copyright (C) 1992, 2000, 2001, 2006 Free Software Foundation, Inc.";
  print ;
  print "AT_BANNER([Documentation examples.])";
  print ;
  print ;
}

/^@node / { # Start a new test group.
  if (seq > 0)
    print "AT_CLEANUP";

  split ($0, tmp, ",");
  node = substr(tmp[1], 7);
  seq = 0;
}

/^@comment file: / { # Produce a data file instead of a test.
  file = $3;
}

/^@comment options: / { # Pass additional options to m4.
  options = $0;
  gsub ("@comment options:", "", options);
}

/^@comment xfail$/ { # Expect the test to fail.
  xfail = 1;
}

/^@comment examples$/ { # The test uses files from the examples dir.
  examples = 1;
}

/^@comment ignore$/ { # This is just formatted doc text, not an actual test.
  getline;
  status = xfail = examples = 0;
  options = file = "";
  next;
}

/^@comment status: / { # Expected exit status of a test.
  status = $3;
}

/^@example$/, /^@end example$/ { # The body of the test.
  if (seq < 0)
    next;

  if ($0 ~ /^@example$/)
    {
      if (seq == 0)
	new_group(node);
      seq++;
      printf ("echo \"$at_srcdir/%s:%d:\"\n", FILENAME, NR)
      next;
    }

  if ($0 ~ /^@end example$/)
    {
      if (file != "")
	{
	  if (output || error)
	    {
	      fatal("while getting file " file      \
		    " found output = " output ","  \
		    " found error = " error);
	    }
	  input = normalize(input);
	  printf ("AT_DATA([[%s]],\n[[%s]])\n\n", file, input);
	}
      else
	{
	  new_test(input, status, output, error, options, xfail, examples);
	}
      status = xfail = examples = 0;
      file = input = output = error = options = "";
      next;
    }

  if ($0 ~ /^\^D$/)
    next;
  if ($0 ~ /^\$ @kbd/)
    next;

  if ($0 ~ /^@result\{\}/)
    output = output $0 "\n";
  else if ($0 ~ /^@error\{\}/)
    error = error $0 "\n";
  else
    input = input $0 "\n";
}

END {
  if (seq > 0)
    print "AT_CLEANUP";
}

# We have to handle CONTENTS line per line, since anchors in AWK are
# referring to the whole string, not the lines.
function normalize(contents,    i, lines, n, line, res) {
  # Remove the Texinfo tags.
  n = split (contents, lines, "\n");
  # We don't want the last field which empty: it's behind the last \n.
  for (i = 1; i < n; ++i)
    {
      line = lines[i];
      gsub (/^@result\{\}/, "", line);
      gsub (/^@error\{\}/,  "", line);
      gsub ("@[{]", "{", line);
      gsub ("@}", "}", line);
      gsub ("@@", "@", line);
      gsub ("@tabchar{}", "\t", line);
      gsub ("@w{ }", " @\\&t@", line);
      gsub ("m4_", "m@\\&t@4_", line);

      # Some of the examples have improperly balanced square brackets.
      gsub ("[[]", "@<:@", line);
      gsub ("[]]", "@:>@", line);

      res = res line "\n";
    }
  return res;
}

function new_group(node) {
  banner = node ". ";
  gsub (/./, "-", banner);
  printf ("\n\n");
  printf ("## %s ##\n", banner);
  printf ("## %s.  ##\n", node);
  printf ("## %s ##\n", banner);
  printf ("\n");
  printf ("AT_SETUP([%s])\n", node);
  printf ("AT_KEYWORDS([[documentation]])\n\n");
}

function new_test(input, status, output, error, options, xfail, examples) {
  input = normalize(input);
  output = normalize(output);
  error = normalize(error);

  if (options ~ /-m/)
    printf ("AT_CHECK_DYNAMIC_MODULE\n");
  if (options ~ /-m mpeval/)
    printf ("AT_CHECK_GMP\n");
  if (xfail == 1)
    printf ("AT_XFAIL_IF([:])\n");

  if (examples == 1)
    {
      printf ("AT_DATA([expout1],\n[[%s]])\n", output);
      printf ("$SED -e \"s|examples|$abs_top_srcdir/examples|g\" \\\n");
      printf ("  < expout1 > expout\n\n");
      if (error)
	{
	  printf ("AT_DATA([experr1],\n[[%s]])\n", error);
	  printf ("$SED \"s|examples|$abs_top_srcdir/examples|g\" \\\n");
	  printf ("  < experr1 > experr\n\n");
	}
      options = options " -I\"$abs_top_srcdir/examples\"";
    }

  printf ("AT_DATA([[input.m4]],\n[[%s]])\n\n", input);
  # Some of these tests `include' files from tests/.
  printf ("AT_CHECK_M4([[%s]], %s,", options, status);
  if (examples == 1)
    printf ("\n[expout]");
  else if (output)
    printf ("\n[[%s]]", output);
  else
    printf (" []");
  if (examples == 1 && error)
    printf (",\n[experr]");
  else if (error)
    printf (",\n[[%s]]", error);
  else
    printf (", []");
  printf (", [[input.m4]])\n\n");
}

function fatal(msg) {
  print "generate.awk: " FILENAME ":" NR ": " msg > "/dev/stderr"
  exit 1
}
