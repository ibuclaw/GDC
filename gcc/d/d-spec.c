/* d-spec.c -- D frontend for GCC.
   Copyright (C) 2011-2013 Free Software Foundation, Inc.

   GCC is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 3, or (at your option) any later
   version.

   GCC is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING3.  If not see
   <http://www.gnu.org/licenses/>.
*/

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"

#include "gcc.h"
#include "opts.h"
#include "simple-object.h"

/* This bit is set if the arguments is a D source file. */
#define DSOURCE		(1<<1)
/* This bit is set if they did `-lm' or `-lmath'.  */
#define MATHLIB		(1<<2)
/* This bit is set if they did `-lpthread'.  */
#define WITHTHREAD	(1<<3)
 /* This bit is set if they did `-lrt'.  */
#define TIMELIB		(1<<4)
/* this bit is set if they did `-lstdc++'.  */
#define WITHLIBCXX	(1<<5)
/* this bit is set if they did `-lc'.  */
#define WITHLIBC	(1<<6)
/* This bit is set when the argument should not be passed to gcc or the backend */
#define SKIPOPT		(1<<8)

#ifndef MATH_LIBRARY
#define MATH_LIBRARY "m"
#endif
#ifndef MATH_LIBRARY_PROFILE
#define MATH_LIBRARY_PROFILE MATH_LIBRARY
#endif

#ifndef THREAD_LIBRARY
#define THREAD_LIBRARY "pthread"
#endif

#ifndef TIME_LIBRARY
#define TIME_LIBRARY ""
#endif

#ifndef LIBSTDCXX
#define LIBSTDCXX "stdc++"
#endif
#ifndef LIBSTDCXX_PROFILE
#define LIBSTDCXX_PROFILE LIBSTDCXX
#endif

#ifndef LIBPHOBOS
#define LIBPHOBOS "gphobos"
#endif
#ifndef LIBPHOBOS_PROFILE
#define LIBPHOBOS_PROFILE LIBPHOBOS
#endif

#ifndef LIBDRUNTIME
#define LIBDRUNTIME "gdruntime"
#endif
#ifndef LIBDRUNTIME_PROFILE
#define LIBDRUNTIME_PROFILE LIBDRUNTIME
#endif

void
lang_specific_driver (cl_decoded_option **in_decoded_options,
		      unsigned int *in_decoded_options_count,
		      int *in_added_libraries)
{
  int i, j;

  /* If nonzero, the user gave us the `-p' or `-pg' flag.  */
  int saw_profile_flag = 0;

  /* Used by -debuglib */
  int saw_debug_flag = 0;

  /* What do with libgphobos:
     -1 means we should not link in libgphobos
     0  means we should link in libgphobos if it is needed
     1  means libgphobos is needed and should be linked in.
     2  means libgphobos is needed and should be linked statically.
     3  means libgphobos is needed and should be linked dynamically. */
  int library = 0;

  /* If nonzero, use the standard D runtime library when linking with
     standard libraries. */
  int phobos = 1;

  /* The number of arguments being added to what's in argv, other than
     libraries.  We use this to track the number of times we've inserted
     -xd/-xnone.  */
  int added = 0;

  /* The new argument list will be contained in this.  */
  cl_decoded_option *new_decoded_options;

  /* "-lm" or "-lmath" if it appears on the command line.  */
  const cl_decoded_option *saw_math = 0;

  /* "-lpthread" if it appears on the command line.  */
  const cl_decoded_option *saw_thread = 0;

  /* "-lrt" if it appears on the command line.  */
  const cl_decoded_option *saw_time = 0;

  /* "-lc" if it appears on the command line.  */
  const cl_decoded_option *saw_libc = 0;

  /* "-lstdc++" if it appears on the command line.  */
  const cl_decoded_option *saw_libcxx = 0;

  /* An array used to flag each argument that needs a bit set for
     DSOURCE, MATHLIB, WITHTHREAD, WITHLIBC or WITHLIBCXX.  */
  int *args;

  /* Whether we need the C++ STD library.  */
  int need_stdcxx = 0;

  /* By default, we throw on the math library if we have one.  */
  int need_math = (MATH_LIBRARY[0] != '\0');

  /* Whether we need the thread library.  */
  int need_thread = (THREAD_LIBRARY[0] != '\0');

  /* By default, we throw on the time library if we have one.  */
  int need_time = (TIME_LIBRARY[0] != '\0');

  /* True if we saw -static. */
  int static_link = 0;

  /* True if we should add -shared-libgcc to the command-line.  */
  int shared_libgcc = 1;

  /* The total number of arguments with the new stuff.  */
  int argc;

  /* The argument list.  */
  cl_decoded_option *decoded_options;

  /* What default library to use instead of phobos */
  const char *defaultlib = NULL;

  /* What debug library to use instead of phobos */
  const char *debuglib = NULL;

  /* The number of libraries added in.  */
  int added_libraries;

  /* The total number of arguments with the new stuff.  */
  int num_args = 1;

  /* "-fonly" if it appears on the command line.  */
  const char *only_source_option = 0;

  /* Whether the -o option was used.  */
  int saw_opt_o = 0;

  /* The first input file with an extension of .d.  */
  const char *first_d_file = NULL;

  argc = *in_decoded_options_count;
  decoded_options = *in_decoded_options;
  added_libraries = *in_added_libraries;

  args = XCNEWVEC (int, argc);

  for (i = 1; i < argc; i++)
    {
      const char *arg = decoded_options[i].arg;

      switch (decoded_options[i].opt_index)
	{
	case OPT_nostdlib:
	case OPT_nodefaultlibs:
	  library = -1;
	  break;

	case OPT_nophoboslib:
	  added = 1; // force argument rebuild
	  phobos = 0;
	  args[i] |= SKIPOPT;
	  break;

	case OPT_defaultlib_:
	  added = 1;
	  phobos = 0;
	  args[i] |= SKIPOPT;
	  if (defaultlib != NULL)
	    free (CONST_CAST (char *, defaultlib));
	  if (arg == NULL)
	    error ("missing argument to 'defaultlib=' option");
	  else
	    {
	      defaultlib = XNEWVEC (char, strlen (arg));
	      strcpy (CONST_CAST (char *, defaultlib), arg);
	    }
	  break;

	case OPT_debuglib_:
	  added = 1;
	  phobos = 0;
	  args[i] |= SKIPOPT;
	  if (debuglib != NULL)
	    free (CONST_CAST (char *, debuglib));
	  if (arg == NULL)
	    error ("missing argument to 'debuglib=' option");
	  else
	    {
	      debuglib = XNEWVEC (char, strlen (arg));
	      strcpy (CONST_CAST (char *, debuglib), arg);
	    }
	  break;

	case OPT_l:
	  if ((strcmp (arg, LIBSTDCXX) == 0)
	      || (strcmp (arg, LIBSTDCXX_PROFILE) == 0))
	    {
	      args[i] |= WITHLIBCXX;
	      need_stdcxx = 0;
	    }
	  else if ((strcmp (arg, MATH_LIBRARY) == 0)
		   || (strcmp (arg, MATH_LIBRARY_PROFILE) == 0))
	    {
	      args[i] |= MATHLIB;
	      need_math = 0;
	    }
	  else if (strcmp (arg, THREAD_LIBRARY) == 0)
	    {
	      args[i] |= WITHTHREAD;
	      need_thread = 0;
	    }
	  else if (strcmp (arg, TIME_LIBRARY) == 0)
	    {
	      args[i] |= TIMELIB;
	      need_time = 0;
	    }
	  else if (strcmp (arg, "c") == 0)
	    args[i] |= WITHLIBC;
	  else
	    /* Unrecognized libraries (e.g. -ltango) may require libphobos.  */
	    library = (library == 0) ? 1 : library;
	  break;

	case OPT_pg:
	case OPT_p:
	  saw_profile_flag++;
	  break;

	case OPT_g:
	  saw_debug_flag = 1;

	case OPT_v:
	  /* If they only gave us `-v', don't try to link in libphobos.  */
	  if (argc == 2)
	    library = 0;
	  break;

	case OPT_x:
	  if (library == 0
	      && (strcmp (arg, "d") == 0))
	    library = 1;
	  break;

	case OPT_Xlinker:
	case OPT_Wl_:
	  /* Arguments that go directly to the linker might be .o files
	     or something, and so might cause libphobos to be needed.  */
	  if (library == 0)
	    library = 1;
	  break;

	case OPT_c:
	case OPT_S:
	case OPT_E:
	case OPT_M:
	case OPT_MM:
	case OPT_fsyntax_only:
	  /* Don't specify libaries if we won't link, since that would
	     cause a warning.  */
	  library = -1;
	  break;

	case OPT_o:
	  saw_opt_o = 1;
	  break;

	case OPT_static:
	  static_link = 1;
	  break;

	case OPT_static_libgcc:
	  shared_libgcc = 0;
	  break;

	case OPT_static_libphobos:
	  library = library >= 0 ? 2 : library;
	  args[i] |= SKIPOPT;
	  break;

	case OPT_shared_libphobos:
	  library = library >= 0 ? 3 : library;
	  args[i] |= SKIPOPT;
	  break;

	case OPT_fonly_:
	  args[i] |= SKIPOPT;
	  only_source_option = decoded_options[i].orig_option_with_args_text;

	  if (arg != NULL)
	    {
	      int len = strlen (only_source_option);
	      if (len <= 2 || only_source_option[len-1] != 'd'
		  || only_source_option[len-2] != '.')
		only_source_option = concat (only_source_option, ".d", NULL);
	    }
	  break;

	case OPT_SPECIAL_input_file:
	    {
	      int len;

	      if (arg[0] == '\0' || arg[1] == '\0')
		continue;

	      len = strlen (arg);
	      /* Record that this is a D source file.  */
	      if (len <= 2 || strcmp (arg + len - 2, ".d") == 0)
		{
		  if (first_d_file == NULL)
		    first_d_file = arg;

		  args[i] |= DSOURCE;
		}

	      /* If we don't know that this is a interface file, we might
		 need to be link against libphobos library.  */
	      if (library == 0)
		{
		  if (len <= 3 || strcmp (arg + len - 3, ".di") != 0)
		    library = 1;
		}

	      /* If this is a C++ source file, we'll need to link
		 against libstdc++ library.  */
	      if ((len <= 3 || strcmp (arg + len - 3, ".cc") == 0)
		  || (len <= 4 || strcmp (arg + len - 4, ".cpp") == 0)
		  || (len <= 4 || strcmp (arg + len - 4, ".c++") == 0))
		need_stdcxx = 1;

	      break;
	    }
	}
    }

  /* If we know we don't have to do anything, bail now.  */
  if (!added && library <= 0 && !only_source_option)
    {
      free (args);
      return;
    }

  /* There's no point adding -shared-libgcc if we don't have a shared
     libgcc.  */
#ifndef ENABLE_SHARED_LIBGCC
  shared_libgcc = 0;
#endif

  /* Make sure to have room for the trailing NULL argument.  */
  /* There is one extra argument added here for the runtime
     library: -lgphobos.  The -pthread argument is added by
     setting need_thread. */
  num_args = argc + added + need_math + shared_libgcc + (library > 0) * 5 + 2;
  new_decoded_options = XNEWVEC (cl_decoded_option, num_args);

  i = 0;
  j = 0;

  /* Copy the 0th argument, i.e., the name of the program itself.  */
  new_decoded_options[j++] = decoded_options[i++];

  /* NOTE: We start at 1 now, not 0.  */
  while (i < argc)
    {
      if (args[i] & SKIPOPT)
	{
	  ++i;
	  continue;
	}

      new_decoded_options[j] = decoded_options[i];

      /* Make sure -lphobos is before the math library, since libphobos
	 itself uses those math routines.  */
      if (!saw_math && (args[i] & MATHLIB) && library > 0)
	{
	  --j;
	  saw_math = &decoded_options[i];
	}

      if (!saw_thread && (args[i] & WITHTHREAD) && library > 0)
	{
	  --j;
	  saw_thread = &decoded_options[i];
	}

      if (!saw_time && (args[i] & TIMELIB) && library > 0)
	{
	  --j;
	  saw_time = &decoded_options[i];
	}

      if (!saw_libc && (args[i] & WITHLIBC) && library > 0)
	{
	  --j;
	  saw_libc = &decoded_options[i];
	}

      if (!saw_libcxx && (args[i] & WITHLIBCXX) && library > 0)
	{
	  --j;
	  saw_libcxx = &decoded_options[i];
	}

      if (args[i] & DSOURCE)
	{
	  if (only_source_option)
	    --j;
	}

      i++;
      j++;
    }

  if (only_source_option)
    {
      const char *only_source_arg = only_source_option + 7;
      generate_option (OPT_fonly_, only_source_arg, 1, CL_DRIVER,
		       &new_decoded_options[j]);
      j++;

      generate_option_input_file (only_source_arg,
				  &new_decoded_options[j++]);
    }

  /* If we are not linking, add a -o option.  This is because we need
     the driver to pass all .d files to cc1d.  Without a -o option the
     driver will invoke cc1d separately for each input file.  */
  if (library < 0 && first_d_file != NULL && !saw_opt_o)
    {
      const char *base;
      int baselen;
      int alen;
      char *out;

      base = lbasename (first_d_file);
      baselen = strlen (base) - 2;
      alen = baselen + 3;
      out = XNEWVEC (char, alen);
      memcpy (out, base, baselen);
      /* The driver will convert .o to some other suffix if appropriate.  */
      out[baselen] = '.';
      out[baselen + 1] = 'o';
      out[baselen + 2] = '\0';
      generate_option (OPT_o, out, 1, CL_DRIVER,
		       &new_decoded_options[j]);
      j++;
    }

  /* Add `-lgphobos' if we haven't already done so.  */
  if (library > 0 && phobos)
    {
      // Default to static linking
      if (library == 1)
        library = 2;

#ifdef HAVE_LD_STATIC_DYNAMIC
      if (library == 3 && static_link)
	{
	  generate_option (OPT_Wl_, LD_DYNAMIC_OPTION, 1, CL_DRIVER,
			   &new_decoded_options[j]);
	  j++;
	}
      else if (library == 2 && !static_link)
	{
	  generate_option (OPT_Wl_, LD_STATIC_OPTION, 1, CL_DRIVER,
			   &new_decoded_options[j]);
	  j++;
	}
#endif

      generate_option (OPT_l, saw_profile_flag ? LIBPHOBOS_PROFILE : LIBPHOBOS, 1,
		       CL_DRIVER, &new_decoded_options[j]);
      added_libraries++;
      j++;
      generate_option (OPT_l, saw_profile_flag ? LIBDRUNTIME_PROFILE : LIBDRUNTIME, 1,
		       CL_DRIVER, &new_decoded_options[j]);
      added_libraries++;
      j++;

#ifdef HAVE_LD_STATIC_DYNAMIC
      if (library == 3 && static_link)
	{
	  generate_option (OPT_Wl_, LD_STATIC_OPTION, 1, CL_DRIVER,
			   &new_decoded_options[j]);
	  j++;
	}
      else if (library == 2 && !static_link)
	{
	  generate_option (OPT_Wl_, LD_DYNAMIC_OPTION, 1, CL_DRIVER,
			   &new_decoded_options[j]);
	  j++;
	}
#endif
    }
  else if (saw_debug_flag && debuglib)
    {
      generate_option (OPT_l, debuglib, 1, CL_DRIVER,
		       &new_decoded_options[j++]);
      added_libraries++;
    }
  else if (defaultlib)
    {
      generate_option (OPT_l, defaultlib, 1, CL_DRIVER,
		       &new_decoded_options[j++]);
      added_libraries++;
    }

  if (saw_libcxx)
    new_decoded_options[j++] = *saw_libcxx;
  else if (library > 0 && need_stdcxx)
    {
      generate_option (OPT_l,
		       (saw_profile_flag
			? LIBSTDCXX_PROFILE
			: LIBSTDCXX),
		       1, CL_DRIVER, &new_decoded_options[j++]);
      added_libraries++;
    }

  if (saw_math)
    new_decoded_options[j++] = *saw_math;
  else if (library > 0 && need_math)
    {
      generate_option (OPT_l,
		       (saw_profile_flag
			? MATH_LIBRARY_PROFILE
			: MATH_LIBRARY),
		       1, CL_DRIVER, &new_decoded_options[j++]);
      added_libraries++;
    }

  if (saw_thread)
    new_decoded_options[j++] = *saw_thread;
  else if (library > 0 && need_thread)
    {
      generate_option (OPT_l, THREAD_LIBRARY, 1, CL_DRIVER,
		       &new_decoded_options[j++]);
      added_libraries++;
    }

  if (saw_time)
    new_decoded_options[j++] = *saw_time;
  else if (library > 0 && need_time)
    {
      generate_option (OPT_l, TIME_LIBRARY, 1, CL_DRIVER,
		       &new_decoded_options[j++]);
      added_libraries++;
    }

  if (saw_libc)
    new_decoded_options[j++] = *saw_libc;

  if (shared_libgcc && !static_link)
    {
      generate_option (OPT_shared_libgcc, NULL, 1, CL_DRIVER,
		       &new_decoded_options[j++]);
    }

  *in_decoded_options_count = j;
  *in_decoded_options = new_decoded_options;
  *in_added_libraries = added_libraries;
}

/* Report an error, if one happened whilst searching for pragma section.  */

static int
find_pragma_error (const char *path, const char *errmsg, int err)
{
  if (errmsg)
    {
      if (err == 0)
	error ("%s: %s", path, errmsg);
      else
	error ("%s: %s: %s", path, errmsg, xstrerror (err));

      return -1;
    }

  return 0;
}

#define PRAGMA_LIB_SECTION_NAME ".d_pragma_libs"

/* Find and return the pragma(lib) section in PBUF and PLEN if one exists
   in the open object file passed in FD.  */

static int
find_pragma_section (int fd, const char *path, char **pbuf, size_t *plen)
{
  const char *errmsg;
  int err;

  *pbuf = NULL;
  *plen = 0;

  /* If we get an error here, just pretend that we didn't find any data.
     This is the right thing to do if the error is that the file was not
     recognized as an object file.  */
  simple_object_read *sobj = simple_object_start_read (fd, 0, "__GNU_D",
						       &errmsg, &err);
  if (sobj == NULL)
    return 0;

  off_t sec_offset;
  off_t sec_length;
  int found = simple_object_find_section (sobj, PRAGMA_LIB_SECTION_NAME,
					  &sec_offset, &sec_length,
					  &errmsg, &err);
  simple_object_release_read (sobj);
  if (!found)
    return find_pragma_error (path, errmsg, err);

  /* The simple_object routine has given us the offset and length of the
     pragma(lib) section in the object file, go retrieve it.  */
  if (lseek (fd, sec_offset, SEEK_SET) < 0)
    return find_pragma_error (path, "lseek failed while reading object data",
			      errno);

  char *buf = XNEWVEC (char, sec_length);
  if (buf == NULL)
    return find_pragma_error (path, "memory allocation failed while reading "
			      "object data", errno);

  ssize_t c = read (fd, buf, sec_length);
  if (c < sec_length)
    {
      free (buf);

      if (c < 0)
	return find_pragma_error (path, "read failed while reading object data",
				  errno);
      else
	return find_pragma_error (path, "short read while reading object data",
				  errno);
    }

  *pbuf = buf;
  *plen = sec_length;

  return 0;
}

/* Called before linking.  Returns 0 on success and -1 on failure.  */

int lang_specific_pre_link()  /* Not used for D.  */
{
  /* Scan all output files, which is just a char** whose only reference
     to it's size is according to n_infiles.  We can't use the driver hook
     `lang_specific_extra_outfiles' because we only know if outfiles needs
     more arguments to be added after compilation has finished.  */
  for (int i = 0; i < n_infiles; i++)
    {
      /* Make a terrible guess at whether this argument is an object file
	 or a linker switch.  */
      if (outfiles[i] == NULL || outfiles[i][0] == '-')
	continue;

      char *buf;
      size_t len;

      int fd = open (outfiles[i], O_RDONLY | O_BINARY);
      if (fd > 0)
	{
	  int err = find_pragma_section (fd, outfiles[i], &buf, &len);
	  close (fd);

	  if (err < 0)
	    return -1;
	}

      /* We found something to scan.  */
      if (buf)
	{
	  char *pbuf = buf;
	  char *end = pbuf + len;

	  while (pbuf < end)
	    {
	      /* Extract the string length, and allocate enough to store
		 it as `-l<string>`.  */
	      size_t namelen = *(size_t *) pbuf;
	      char *name = (char *) alloca (namelen + 2);
	      size_t j;

	      /* Make sure that we never read past the end of buffer.  */
	      if (pbuf + namelen > end)
		return find_pragma_error (outfiles[i], "corrupted data in "
					  "pragma(lib) section", 0);

	      /* We are now looking at the string, copy it.  */
	      pbuf += sizeof (size_t);

	      for (j = 0; j < namelen; j++)
		name[2 + j] = pbuf[j];

	      /* Prefix `-l` and add null terminator at the end.  */
	      name[0] = '-';
	      name[1] = 'l';
	      name[namelen + 2] = '\0';

	      /* Append linker option to end of outfiles.  */
	      outfiles = XRESIZEVEC (const char *, outfiles, ++n_infiles);
	      outfiles[n_infiles - 1] = XNEWVEC (char, namelen + 2);
	      strcpy (CONST_CAST (char *, outfiles[n_infiles - 1]), name);

	      pbuf += namelen;
	    }

	  free (buf);
	}
    }
  return 0;
}

/* Number of extra output files that lang_specific_pre_link may generate.  */

int lang_specific_extra_outfiles = 0;  /* Not used for D.  */

