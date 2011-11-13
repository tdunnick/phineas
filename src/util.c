/*
 * util.c
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
 *
 * general use stuff...
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <windows.h>
#include "util.h"

/*
 * some useful extra string functions
 */
char *stralloc (char *old, char *new)
{
  old = (char *) realloc (old, strlen (new) + 1);
  return (strcpy (old, new));
}

/*
 * search for string in a string upto len
 */
char *strnstr (char *haystack, char *needle, int len)
{
  while (len--)
  {
    if (strstarts (haystack, needle))
      return (haystack);
    haystack++;
  }
  return (NULL);
}

/*
 * return true if string starts with prefix
 */
int strstarts (char *s, char *prefix)
{
  while (*prefix)
  {
    if (*s++ != *prefix++)
      return (0);
  }
  return (1);
}

/*
 * return true if string has nothing "visible"
 */
int strempty (char *s)
{
  if (s == NULL)
    return (1);
  while (*s)
  {
    if (!isspace (*s))
      return (0);
    s++;
  }
  return (1);
}

/*
 * PHINMS formatted times
 * TODO thread safety for localtime...
 */

#define PTIMEFMT "%Y-%m-%dT%H:%M:%S"

char *ptime (time_t *t, char *buf)
{
  time_t tv;
  struct tm *tm;
  if (t == NULL)
  {
    t = &tv;
    time (t);
  }
  tm = localtime (t);
  strftime (buf, PTIMESZ, PTIMEFMT, tm);
  return (buf);
}

/*
 * PHINMS process ID's
 * TODO thread safety for previous PID
 * Psuedo millisecond granular by using the MS tick counter...
 */

time_t util_tv = 0;
int util_ms = 0;

char *ppid (char *buf)
{
  time_t tv;
  int ms;

  time (&tv);
  ms = GetTickCount () % 1000;
  if ((tv == util_tv) && (ms == util_ms))
    ms++;
  util_tv = tv;
  util_ms = ms;
  sprintf (buf, "%ld%03d", tv, ms);
  return (buf);
}

/*
 * Return a pointer to the base file name of a path
 */
char *basename (char *path)
{
  char *bs, *fs;

  bs = strrchr (path, '\\');
  fs = strrchr (path , '/');
  if (bs > fs)
    return (bs + 1);
  if (fs > bs)
    return (fs + 1);
  return (path);
}

/*
 * read and return file contents
 */
unsigned char *readfile (char *path, int *l)
{
  struct stat st;
  unsigned char *ch;
  FILE *fp;
  int len;

  if (stat (path, &st))
    return (NULL);
  if ((fp = fopen (path, "rb")) == NULL)
    return (NULL);
  ch = (unsigned char *) malloc (st.st_size + 1);
  len = fread (ch, 1, st.st_size, fp);
  ch[len] = 0;
  if (l != NULL)
    *l = len;
  fclose (fp);
  return (ch);
}

/*
 * write file contents
 */
int writefile (char *path, unsigned char *buf, int sz)
{
  FILE *fp;

  if ((fp = fopen (path, "wb")) == NULL)
    return (-1);
  sz = fwrite (buf, 1, sz, fp);
  fclose (fp);
  return (sz);
}


/*
 * set a loadpath - underlying directory must exist!
 */
char LoadPath[MAX_PATH] = "";		/* where we loaded from		*/

char *loadpath (char *prog)
{
  char *ch;
  struct stat st;

  if (prog != NULL)
  {
    _fullpath (LoadPath, prog, MAX_PATH);
    while (stat (LoadPath, &st) || S_ISREG (st.st_mode))
    {
      if ((ch = strrchr (LoadPath, DIRSEP)) == NULL)
      {
	*LoadPath = 0;
	break;
      }
      *ch = 0;
    }
    if (*LoadPath)
    {
      ch = LoadPath + strlen (LoadPath);
      *ch++ = DIRSEP;
      *ch = 0;
    }
  }
  return (LoadPath);
}

/*
 * Fix a path for MS.  Prepend the LoadPath if path is relative.
 * Return -1 if path is too big, or path size
 */
int fixpath (char *p)
{
  char *ch;
  int l, lp;

  lp = strlen (p);
  for (ch = p; *ch; ch++)
  {
    if (*ch == '/') *ch = DIRSEP;
  }
  if (p == LoadPath)
    return (lp);
  if (p[1] == ':')			/* could be relative	*/
    return (lp);			/* but punt anyway	*/
  if (p[0] == DIRSEP)
  {
    if (p[1] == DIRSEP) 		/* a UNC path 		*/
      return (lp);
    if (lp > MAX_PATH - 3)		/* no room		*/
      return (-1);
    memmove (p + 2, p, strlen (p));	/* add drive letter	*/
    p[0] = LoadPath[0];
    p[1] = ':';
    return (lp + 2);
  }
  l = strlen (LoadPath);
  if (l + lp >= MAX_PATH)
    return (-1);
  memmove (p + l, p, lp + 1);
  memmove (p, LoadPath, l);
  return (lp + l);
}


/*
 * copy a path, limited to MAX_PATH - return copy length
 */
int pathcopy (char *dst, char *src)
{
  int l = strlen (src);
  if (l >= MAX_PATH)
    return (-1);
  strcpy (dst, src);
  return (l);
}

/*
 * format a path for use
 */
char *pathf (char *dst, char *fmt, ...)
{
  int l;
  va_list ap;

  va_start (ap, fmt);
  l = vsnprintf (dst, MAX_PATH, fmt, ap);
  va_end (ap);
  if ((l < 0) || (fixpath (dst) < 0))
    return (NULL);
  return (dst);
}

/*************************** HTTP basics ***************************/

/*
 * covert two hex digits to a value
 */
int gethex (int c)
{
  c = toupper (c);
  if (c > '9')
    return (c + 10 - 'A');
  return (c - '0');
}

/*
 * decode a url
 */
char *urldecode (char *url)
{
  char *src, *dst;
  
  src = dst = url;
  while (*src)
  {
    if (*src == '%')
    {
      *dst = gethex (src[1]) * 16 + gethex (src[2]);
      src += 3;
    }
    else if ((*dst = *src++) == '+')
      *dst = ' ';
    /* if (*dst != '\r')		 remove carrage returns?	*/
    dst++;
  }
  *dst = 0;
  return (url);
}

/*
 * encode html special chars
 */
char *html_encode (char *dst, char *src)
{
  char *p;

  if ((dst == NULL) || (src == NULL))
    return (NULL);
  p = dst;
  while (*src)
  {
    switch (*src)
    {
      case '>' : strcpy (p, "&gt;"); p += 4; break;
      case '<' : strcpy (p, "&lt;"); p += 4; break;
      case '&' : strcpy (p, "&amp;"); p += 5; break;
      case '"' : strcpy (p, "&quot;"); p += 6; break;
      default : *p++ = *src; break;
    }
    src++;
  }
  *p = 0;
  return (dst);
}

/*
 * rename file to backup with timestamp extension
 */
int backup (char *fname)
{
  struct stat st;
  time_t t;
  char path[MAX_PATH];

  if (stat (fname, &st))		/* nothin' to do...	*/
    return (0);
  time (&t);
  do
  {
    sprintf (path, "%s.%ld", fname, t);
    t++;
  } while (!stat (path, &st));
  return (rename (fname, path));
}

#ifdef FOOBAR

/*
 * remove trailing white space from string
 */
char *trailing (char *s, int l)
{
  if (l == 0) l = strlen (s);
  while (l-- && isspace(s[l]));
  s[l+1] = 0;
  return (s);
}


/*
 * case insensitive match prefix
 * return index of first non-matching char or EOS
 */

int stripre (char *s, char *prefix)
{
  int i;

  for (i = 0; prefix[i]; i++)
  {
    if (toupper (s[i]) != toupper (prefix[i]))
      break;
  }
  return (i);
}

/*
 * case insensitive string search
 */
char *stristr (char *s1, char *s2)
{
  int i;

  while (*s1)
  {
    for (i = 0; toupper (s1[i]) == toupper (s2[i]); i++)
    {
      if (s2[i + 1] == 0)
	return (s1);
    }
    s1++;
  }
  return (NULL);
}

/*
 * make a match replacement - see match below
 * enter with pc at current replacement point.
 * return next replacement point
 * Don't let replacement exceed MAX_PATH
 */
void match_replace (int card, int len, char *s, char *rep)
{
  char *pc;

  if ((rep == NULL) || (card && ((len <= 0) || (s == NULL))))
    return (0);
  pc = rep;
  while ((pc = strchr (pc, '%')) != NULL)
  {
    if ((card == 0) && isdigit (pc[1]))	/* remove		*/
      memmove (pc, pc + 2, strlen (pc) - 1);
    else if (pc[1] - '0' == card)	/* substitute		*/
    {
      if (len == 1)
	memmove (pc, pc + 1, strlen (pc));
      else if (len > 2)
      {
	if (strlen (rep) + len >= MAX_PATH)
	  break;
        memmove (pc + len - 2, pc, strlen (pc) + 1);
      }
      memmove (pc, s, len);
    }
    else
      pc++;
  }
}

/*
 * simple (URL) pattern matching and substitution
 */

int match (char *pat, char *s, char *rep)
{
  char *wild;
  char *beg;
  int card = 0;

  while (*pat)
  {
    if ((wild = strchr (pat + 1, '*')) == NULL)
      wild = pat + strlen (pat);
    if (*pat == '*')			/* match from wild card	*/
    {
      card++;
      if (*++pat == 0)			/* ending wildcard	*/
      {
	match_replace (card, strlen (s), s, rep);
	s += strlen (s);
	break;
      }
      if (pat == wild)			/* skip adjacent	*/
	continue;
      beg = s;				/* note beginning	*/
      while (strnicmp (s, pat, wild - pat))
      {
	if (*s++ == 0)
	  return (0);
      }
      match_replace (card, s - beg, beg, rep);
    }
    else				/* match exactly	*/
    {
      if (strnicmp (pat, s, wild - pat))
        return (0);
    }
    s += wild - pat;			/* part that matched	*/
    pat = wild;
  }
  if (*s)				/* not all matchdd	*/
    return (0);
  match_replace (0, 0, NULL, rep);
  return (1);
}

#endif /* FOOBAR */

#ifdef UNITTEST
#include "unittest.h"
#define debug _DEBUG_

int main (int argc, char **argv)
{
  char b[PTIMESZ];

  debug ("ptime() %s\n", ptime (NULL, b));
  info ("%s unit test completed\n", argv[0]);
  exit (0);
}

#endif
