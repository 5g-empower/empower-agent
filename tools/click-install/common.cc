/*
 * common.cc -- common code for click-install and click-uninstall
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2002 International Computer Science Institute
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>

#include <click/glue.hh>
#include "common.hh"
#include "toolutils.hh"
#include <unistd.h>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#if FOR_BSDMODULE
# include <sys/param.h>
# include <sys/mount.h>
#elif FOR_LINUXMODULE && HAVE_CLICKFS
# include <sys/mount.h>
#endif

#if FOR_BSDMODULE || (FOR_LINUXMODULE && HAVE_CLICKFS)
const char *clickfs_prefix = "/click";
#elif FOR_LINUXMODULE
const char *clickfs_prefix = "/proc/click";
#endif

bool verbose = false;

static void
read_package_string(const String &text, StringMap &packages)
{
  const char *s = text.data();
  int pos = 0;
  int len = text.length();
  while (pos < len) {
    int start = pos;
    while (pos < len && !isspace(s[pos]))
      pos++;
    packages.insert(text.substring(start, pos - start), 0);
    pos = text.find_left('\n', pos) + 1;
  }
}

bool
read_package_file(String filename, StringMap &packages, ErrorHandler *errh)
{
  if (!errh && access(filename.c_str(), F_OK) < 0)
    return false;
  int before = errh->nerrors();
  String str = file_string(filename, errh);
  if (!str && errh->nerrors() != before)
    return false;
  read_package_string(str, packages);
  return true;
}

bool
read_active_modules(StringMap &packages, ErrorHandler *errh)
{
#if FOR_LINUXMODULE
  return read_package_file("/proc/modules", packages, errh);
#else
  int before = errh->nerrors();
  String output = shell_command_output_string
    ("/sbin/kldstat | /usr/bin/awk \'/Name/ {\n"
     "  for (i = 1; i <= NF; i++)\n"
     "    if ($i == \"Name\")\n"
     "      n = i;\n"
     "  next;\n"
     "}\n"
     "{ print $n }\'", String(), errh);
  if (!output && errh->nerrors() != before)
    return false;
  read_package_string(output, packages);
  return true;
#endif
}

static int
kill_current_configuration(ErrorHandler *errh)
{
  if (verbose)
    errh->message("Installing blank configuration in kernel");

  String clickfs_config = clickfs_prefix + String("/config");
  String clickfs_threads = clickfs_prefix + String("/threads");
  
  FILE *f = fopen(clickfs_config.c_str(), "w");
  if (!f)
    errh->fatal("cannot uninstall configuration: %s", strerror(errno));
  fputs("// nothing\n", f);
  fclose(f);

  // wait for thread to die
  if (verbose)
    errh->message("Waiting for Click threads to die");
  for (int wait = 0; wait < 6; wait++) {
    String s = file_string(clickfs_threads);
    if (!s)
      return 0;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    select(0, 0, 0, 0, &tv);
  }
  
  return errh->error("failed to kill current Click configuration");
}

int
remove_unneeded_packages(const StringMap &active_modules, const StringMap &packages, ErrorHandler *errh)
{
  // remove extra packages
  Vector<String> removals;
  
  // go over all modules; figure out which ones are Click packages
  // by checking `packages' array; mark old Click packages for removal
  for (StringMap::iterator iter = active_modules.begin(); iter; iter++)
    // only remove packages that weren't used in this configuration.
    // packages used in this configuration have value > 0
    if (iter.value() == 0) {
      String key = iter.key();
      if (packages[key] >= 0)
	removals.push_back(key);
      else {
	// check for removing an old archive package;
	// they are identified by a leading underscore.
	int p;
	for (p = 0; p < key.length() && key[p] == '_'; p++)
	  /* nada */;
	String s = key.substring(p);
	if (s && packages[s] >= 0)
	  removals.push_back(key);
	else if (s.length() > 3 && s.substring(s.length() - 3) == OBJSUFFIX) {
	  // check for .ko/.bo packages
	  s = s.substring(0, s.length() - 3);
	  if (s && packages[s] >= 0)
	    removals.push_back(key);
	}
      }
    }

  // actually remove the packages
  int retval = 0;
  if (removals.size()) {
    String to_remove;
    for (int i = 0; i < removals.size(); i++)
      to_remove += " " + removals[i];
    if (verbose)
      errh->message("Removing packages:%s", to_remove.cc());

#if FOR_LINUXMODULE
    String cmdline = "/sbin/rmmod" + to_remove + " 2>/dev/null";
    int status = system(cmdline.c_str());
    if (status < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
      retval = errh->error("cannot remove package(s) '%s'", to_remove.c_str());
#elif FOR_BSDMODULE
    for (int i = 0; i < removals.size(); i++) {
      String cmdline = "/sbin/kldunload " + removals[i];
      int status = system(cmdline.c_str());
      if (status < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
	retval = errh->error("cannot remove package '%s'", removals[i].c_str());
    }
#endif
  }
  return retval;
}

int
unload_click(ErrorHandler *errh)
{
  String clickfs_packages = clickfs_prefix + String("/packages");
  
  // do nothing if Click not installed
  if (access(clickfs_packages.c_str(), F_OK) < 0)
    return 0;
  
  // first, write nothing to /proc/click/config -- frees up modules
  if (kill_current_configuration(errh) < 0)
    return -1;

  // find current packages
  HashMap<String, int> active_modules(-1);
  HashMap<String, int> packages(-1);
  read_active_modules(active_modules, errh);
  read_package_file(clickfs_prefix + String("/packages"), packages, errh);

  // remove unused packages
  (void) remove_unneeded_packages(active_modules, packages, errh);

#if FOR_BSDMODULE
  // unmount Click file system
  if (verbose)
    errh->message("Unmounting Click filesystem at %s", clickfs_prefix);
  int unmount_retval = unmount(clickfs_prefix, MNT_FORCE);
  if (unmount_retval < 0)
    errh->error("cannot unmount %s: %s", clickfs_prefix, strerror(errno));
#endif

  // remove Click module
  if (verbose)
    errh->message("Removing Click module");
  int status;
#if FOR_LINUXMODULE
  status = system("/sbin/rmmod click");
#elif FOR_BSDMODULE
  status = system("/sbin/kldunload click.ko");
#endif
  if (status < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
    return errh->error("cannot remove Click module from kernel");

#if FOR_LINUXMODULE && HAVE_CLICKFS
  // proclikefs will take care of the unmount for us, but we'll give it a shot
  // anyway.
  if (verbose)
    errh->message("Unmounting Click filesystem at %s", clickfs_prefix);
  (void) umount(clickfs_prefix);
#endif

  // see if we successfully removed it
  if (access(clickfs_packages.c_str(), F_OK) >= 0) {
    errh->warning("cannot uninstall Click module");
    return -1;
  }
  return 0;
}
