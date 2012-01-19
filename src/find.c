/*
 * find.c
 *
 * Copyright 2011-2012 Thomas L Dunnick
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 * FIND file on our file system
 * 03-11-10 T. Dunnick
 */

#ifdef UNITTEST
#define __SENDER__
#endif

#ifdef __SENDER__

#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <sys/stat.h>
#include "find.h"

#ifdef UNITTEST
#include "unittest.h"
#define debug _DEBUG_
#endif

#ifndef debug
#define debug(fmt...)
#endif

#define _SKIPFILE_(e) ((e)->attrib&(_A_SYSTEM|_A_VOLID|_A_HIDDEN|_A_SUBDIR))
#define _ISDIR_(e) ((e)->attrib&_A_SUBDIR)

/*
 * finds the last slash in a directory path
 */
char *find_lastslash (char *path)
{
  char *ch, *ch2;

  ch = strrchr (path, '\\');
  ch2 = strrchr (path, '/');
  if (ch == NULL)
    return (ch2);
  if (ch2 == NULL)
    return (ch);
  return (ch < ch2 ? ch2 : ch);
}

/*
 * return a path with associated file pattern for a subdirectory
 */
char *find_subpath (char *path, char *pat, char *subdir)
{
  char *ch = find_lastslash (pat);
  int len = 0;

  if (ch != NULL)
  {
    len = ++ch - pat;
    strncpy (path, pat, len);
  }
  else
    ch = pat;
  strcpy (path + len, subdir);
  strcat (path, "/");
  strcat (path, ch);
  return (path);
}

/*
 * construct a path to the file name from a pattern
 */
char *find_pathname (char *path, char *pat, char *name)
{
  int len = 0;
  char *ch = find_lastslash (pat);

  if (ch != NULL)
  {
    len = ++ch - pat;
    strncpy (path, pat, len);
  }
  strcpy (path + len, name);
  return (path);
}

/*
 * update path for wildcards if it represented a folder
 */
char *find_folder (char *path, char *pat)
{
  struct stat st;
  char *ch;

  if ((pat == NULL) || (*pat == 0))
  {
    strcpy (path, "*");
    return (path);
  }
  strcpy (path, pat);
  if (((ch = find_lastslash (path)) != NULL) && (ch[1] == 0))
    *ch = 0;
  if ((stat (path, &st) == 0) && (st.st_mode & _S_IFDIR))
    pat = strcat (path, "/*");
  return (pat);
}

/*
 * Return a list of files that match this pattern.  Optionally
 * recurse through directories.
 */

FINDNAME *find (char *pat, int recurse)
{
  struct _finddata_t file;
  int handle;
  char dirpath[256];
  FINDNAME **new, *fnames;

  new = &fnames;
  if (find_folder (dirpath, pat) != pat)
    return (find (dirpath, recurse));
				/* for subdirectories		*/
  if (recurse && (recurse++ < 5))
  {
    handle = _findfirst (find_pathname (dirpath, pat, "*"), &file);
    while (handle >= 0)
    {
      if (_ISDIR_(&file) && (file.name[0] != '.'))
      {
	debug ("recurse %s\n", find_subpath (dirpath, pat, file.name));
	*new = find (find_subpath (dirpath, pat, file.name), recurse);
	while (*new != NULL)
	  new = &(*new)->next;
      }
      if (_findnext (handle, &file) < 0)
      {
        _findclose (handle);
        handle = -1;
      }
    }
  }
  debug ("searching pattern %s\n", pat);

  				/* for this pattern		*/
  handle = _findfirst (pat, &file);
  debug ("first file found is %s\n", handle >= 0  ? file.name : "NULL");
  while (handle >= 0)
  {
    if (!_SKIPFILE_(&file))
    {
      find_pathname (dirpath, pat, file.name);
      debug ("adding %s\n", file.name);
      *new = (FINDNAME *) malloc (sizeof (FINDNAME) + strlen (dirpath));
      strcpy ((*new)->name, dirpath);
      new = &(*new)->next;
    }
    if (_findnext (handle, &file) < 0)
    {
      _findclose (handle);
      handle = -1;
    }
  }
  *new = NULL;
  return (fnames);
}

/*
 * Copy and dispose of next file in the list
 */
char *find_next (FINDNAME **fnames, char *name)
{
  FINDNAME *f;
  if ((f = *fnames) == NULL)
    return NULL;
  *fnames = f->next;
  //debug ("nextfile is %s\n", f->name);
  strcpy (name, f->name);
  free (f);
  return (name);
}

/*
 * get rid of any leftovers
 */
FINDNAME *find_free (FINDNAME *fnames)
{
  FINDNAME *t;
  while (fnames != NULL)
  {
    t = fnames;
    fnames = t->next;
    free (t);
  }
  return (NULL);
}

#ifdef UNITTEST
#include <stdio.h>

int main (int argc, char **argv)
{
  FINDNAME *f;
  char fname[MAX_PATH];

  if (argc > 1)
    f = find (argv[1], 1);
  else
    f = find ("", 1);
  while (find_next (&f, fname) != NULL)
    printf ("%s\n", fname);
  info ("%s unit test completed\n", *argv);
}

#endif
#endif /* __SENDER__ */
