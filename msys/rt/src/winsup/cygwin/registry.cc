/* registry.cc: registry interface

   Copyright 1996, 1997, 1998, 1999, 2000, 2001 Red Hat, Inc.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#include "winsup.h"
#include "shared_info.h"
#include "registry.h"
#include "security.h"
#include <cygwin/version.h>
#include <errno.h>
#include "cygerrno.h"

static char NO_COPY cygnus_class[] = "cygnus";

reg_key::reg_key (HKEY top, REGSAM access, ...)
{
  va_list av;
  va_start (av, access);
  build_reg (top, access, av);
  va_end (av);
}

reg_key::reg_key (REGSAM access, ...)
{
  va_list av;

  new (this) reg_key (HKEY_CURRENT_USER, access, "SOFTWARE",
		 "MinGW",
		 "MSYS", NULL);

  HKEY top = key;
  va_start (av, access);
  build_reg (top, KEY_READ, av);
  va_end (av);
  if (top != key)
    RegCloseKey (top);
}

reg_key::reg_key (REGSAM access)
{
  new (this) reg_key (HKEY_CURRENT_USER, access, "SOFTWARE",
		 REGISTRY_PROJECT_NAME,
		 REGISTRY_PACKAGE_NAME,
		 REGISTRY_PROGRAM_NAME, NULL);
}

void
reg_key::build_reg (HKEY top, REGSAM access, va_list av)
{
  set_errno(ENOSYS);
}

/* Given the current registry key, return the specific int value
   requested.  Return def on failure. */

int
reg_key::get_int (const char *name, int def)
{
  DWORD type;
  DWORD dst;
  DWORD size = sizeof (dst);

  if (key_is_invalid)
    return def;

  LONG res = RegQueryValueExA (key, name, 0, &type, (unsigned char *) &dst,
			       &size);

  if (type != REG_DWORD || res != ERROR_SUCCESS)
    return def;

  return dst;
}

/* Given the current registry key, set a specific int value. */

int
reg_key::set_int (const char *name, int val)
{
  DWORD value = val;
  if (key_is_invalid)
    return key_is_invalid;

  return (int) RegSetValueExA (key, name, 0, REG_DWORD,
			       (unsigned char *) &value, sizeof (value));
}

/* Given the current registry key, return the specific string value
   requested.  Return zero on success, non-zero on failure. */

int
reg_key::get_string (const char *name, char *dst, size_t max, const char * def)
{
  DWORD size = max;
  DWORD type;
  LONG res;

  if (key_is_invalid)
    res = key_is_invalid;
  else
    res = RegQueryValueExA (key, name, 0, &type, (unsigned char *) dst, &size);

  if ((def != 0) && ((type != REG_SZ) || (res != ERROR_SUCCESS)))
    strcpy (dst, def);
  return (int) res;
}

/* Given the current registry key, set a specific string value. */

int
reg_key::set_string (const char *name, const char *src)
{
  if (key_is_invalid)
    return key_is_invalid;
  return (int) RegSetValueExA (key, name, 0, REG_SZ, (unsigned char*) src,
			       strlen (src) + 1);
}

/* Return the handle to key. */

HKEY
reg_key::get_key ()
{
  return key;
}

/* Delete subkey of current key.  Returns the error code from the
   RegDeleteKeyA invocation. */

int
reg_key::kill (const char *name)
{
  if (key_is_invalid)
    return key_is_invalid;
  return RegDeleteKeyA (key, name);
}

/* Delete the value specified by name of current key.  Returns the error code
   from the RegDeleteValueA invocation. */

int
reg_key::killvalue (const char *name)
{
  if (key_is_invalid)
    return key_is_invalid;
  return RegDeleteValueA (key, name);
}

reg_key::~reg_key ()
{
  if (!key_is_invalid)
    RegCloseKey (key);
  key_is_invalid = 1;
}

char *
get_registry_hive_path (const PSID psid, char *path)
{
  char sid[256];
  char key[256];
  HKEY hkey;

  if (!psid || !path)
    return NULL;
  cygsid csid (psid);
  csid.string (sid);
  strcpy (key,"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList\\");
  strcat (key, sid);
  if (!RegOpenKeyExA (HKEY_LOCAL_MACHINE, key, 0, KEY_READ, &hkey))
    {
      char buf[256];
      DWORD type, siz;

      key[0] = '\0';
      if (!RegQueryValueExA (hkey, "ProfileImagePath", 0, &type,
			     (BYTE *)buf, (siz = 256, &siz)))
	ExpandEnvironmentStringsA (buf, key, 256);
      RegCloseKey (hkey);
      if (key[0])
	return strcpy (path, key);
    }
  return NULL;
}

void
load_registry_hive (PSID psid)
{
  char sid[256];
  char path[MAX_PATH + 1];
  HKEY hkey;
  LONG ret;

  if (!psid)
    return;
  /* Check if user hive is already loaded. */
  cygsid csid (psid);
  csid.string (sid);
  if (!RegOpenKeyExA (HKEY_USERS, csid.string (sid), 0, KEY_READ, &hkey))
    {
      debug_printf ("User registry hive for %s already exists", sid);
      RegCloseKey (hkey);
      return;
    }
  if (get_registry_hive_path (psid, path))
    {
      strcat (path, "\\NTUSER.DAT");
      if ((ret = RegLoadKeyA (HKEY_USERS, sid, path)) != ERROR_SUCCESS)
	debug_printf ("Loading user registry hive for %s failed: %d", sid, ret);
    }
}

