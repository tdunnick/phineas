/*
 * find.h
 *
 * Copyright 2011 Thomas L Dunnick
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
 */
#ifndef __FIND__
#define __FIND__

typedef struct findname
{
  struct findname *next;
  char name[1];
} FINDNAME;

char *find_lastslash (char *path);
char *find_subpath (char *path, char *pat, char *subdir);
char *find_pathname (char *path, char *pat, char *name);
char *find_folder (char *path, char *pat);
FINDNAME *find (char *pat, int recurse);
char *find_next (FINDNAME **fnames, char *name);
FINDNAME *find_free (FINDNAME *fnames);

#endif /* __FIND__ */
