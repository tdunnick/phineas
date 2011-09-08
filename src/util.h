/*
 * util.h
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
#ifndef __UTIL__
#define __UTIL__

#include <time.h>

#define PTIMESZ 22
#define DIRSEP '\\'

char *stralloc (char *old, char *new);
char *strnstr (char *haystack, char *needle, int len);
int strstarts (char *s, char *prefix);
char *ptime (time_t *t, char *buf);
char *ppid (char *buf);
char *basename (char *path);
unsigned char *readfile (char *path, int *len);
int writefile (char *path, unsigned char *buf, int len);
char *loadpath (char *path);
int fixpath (char *p);
int pathcopy (char *dst, char *src);
char *pathf (char *dst, char *fmt, ...);
/*
 * decode a url in place
 */
char *urldecode (char *url);
char *html_encode (char *dst, char *src);
/*
 * rename file to backup with timestamp extension
 */
int backup (char *fname);

#endif /* __UTIL__ */
