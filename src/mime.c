/*
 * mime.c
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
 */
#include <stdlib.h>

#ifdef UNITTEST
#undef UNITTEST
#include <sys/stat.h>
#include "unittest.h"
#include "dbuf.c"
#define UNITTEST
#define debug _DEBUG_
#endif

#include "log.h"
#include "mime.h"
#include "dbuf.h"

#ifndef debug
#define debug(fmt...)
#endif

int mime_format_part (MIME *mime, DBUF *b);

/*
 * Mime boundarys start with a newline, two dashes, and the boundary string
 * The last boundary is followed by two dashes.
 */


/*
 * allocate a mime structure
 */
MIME *mime_alloc ()
{
  MIME *m = (MIME *) malloc (sizeof (MIME));
  memset (m, 0, sizeof (MIME));
  return (m);
}

/*
 * free a mime structure including all parts of a multipart
 */
MIME *mime_free (MIME *m)
{
  if (m == NULL)
    return;
  mime_free (m->next);
  if (m->headers != NULL)
    free (m->headers);
  if (m->body != NULL)
    free (m->body);
  free (m);
  return (NULL);
}

/*
 * Set a header, preferably at the position given.  Return actual place.
 * Use a big pos to force it to append.
 */
int mime_setHeader (MIME *m, char *name, char *value, int pos)
{
  int c, p, sz;
  char *ch, *nl;

  debug ("old headers...\n%s", m->headers);
  /* 
   * add colon, space, CR, and NL to needed size
   */
  sz = strlen (name) + strlen (value) + 4;
  /* 
   * easy if this is the only header
   */
  if (m->headers == NULL)
  {
    m->headers = (char *) malloc (sz + 3);
    sprintf (m->headers, "%s: %s\r\n\r\n", name, value);
    return (0);
  }
  /* 
   * if already set, remove it...
   */
  if ((ch = strstr (m->headers, name)) != NULL)
  {
    if ((nl = strchr (ch, '\n')) != NULL)
      strcpy (ch, nl + 1);
    debug ("removed %s resuling header...\n%s", name, m->headers);
  }
  debug ("realloc...\n%s", m->headers);
  m->headers = (char *) realloc (m->headers, sz + strlen (m->headers) + 1);
  debug ("after...\n%s", m->headers);
  /*
   * find insertion point
   */
  ch = m->headers;
  for (p = 0; p < pos; p++)
  {
    if ((nl = strchr (ch, '\n')) == NULL)
      return (-1);
    ch = nl + 1;
    if ((*ch == '\n') || !strcmp (ch, "\r\n"))
      break;
  }
  memmove (ch + sz, ch, strlen (ch) + 1);
  c = ch[sz];
  sprintf (ch, "%s: %s\r\n", name, value);
  ch[sz] = c;
  debug ("new headers...\n%s", m->headers);
  return (p);
}

/*
 * get Content-Length
 */
int mime_getLength (MIME *m)
{
  char *ch = mime_getHeader (m, MIME_LENGTH);
  if (ch == NULL)
    return (0);
  return (atoi (ch));
}

/*
 * set Content-Length
 */
int mime_setLength (MIME *m, int len)
{
  char buf[10];

  sprintf (buf, "%d", len);
  return (mime_setHeader (m, MIME_LENGTH, buf, 1));
}

/*
 * get the value location of a mime header, delimited by '\r\n'
 */
char *mime_getHeader (MIME *m, char *name)
{
  char *ch;

  if ((m->headers == NULL) || 
     ((ch = strstr (m->headers, name)) == NULL) ||
     ((ch = strchr (ch, ':')) == NULL))
  {
    debug ("header %s not found\n", name);
    return (NULL);
  }
  do ch++; while (isspace (*ch) && (*ch != '\n'));
  debug ("header %s %.10s...\n", name, ch);
  return (ch);
}

/*
 * return a pointer to the body of this message
 */
unsigned char *mime_getBody (MIME *m)
{
  return (m->body);
}

/*
 * copy the body from a buffer, set Content-Length, and return buffer len
 * if incoming len < 1, use the buffer string length
 */
int mime_setBody (MIME *m, unsigned char *b, int len)
{
  if (len < 1)
    len = strlen (b);
  mime_setLength (m, m->len = len);
  m->body = (unsigned char *) realloc (m->body, len);
  memcpy (m->body, b, len);
  return (len);
}

/*
 * add a multipart chunk - multipart must be set
 */
MIME *mime_setMultiPart (MIME *m, MIME *part)
{
  MIME **n;
  if (mime_getBoundary (m, NULL, 0) < 1)
    return (NULL);
  for (n = &m->next; *n != NULL; n = &(*n)->next);
  *n = part;
  return (m);
}

/*
 * set a mulitpart mime boundary - the current time and a random
 * number is used to set the boundary text
 */
int mime_setBoundary (MIME *m, char *attr)
{
  char b[256];

  snprintf (b, 256, "%s; %s boundary=\"_Part_%ld_%d\"", 
    MIME_MULTIPART, attr, time (NULL), rand ());
  return (mime_setHeader (m, MIME_CONTENT, b, 0));
}

/*
 * Get and format a boundary if there. Return it's size, -1 if too big
 * or 0 if not found.  If buf is not NULL, copy the boundary to it
 * to used in searching for MIME multi-parts.
 */
int mime_getBoundary (MIME *m, char *buf, int sz)
{
  char *p, *ch;
  int l;

  if (buf != NULL)
    *buf = 0;
  if ((p = mime_getHeader (m, MIME_CONTENT)) == NULL)
  {
    debug ("no Content-Type found\n");
    return (0);
  }
  l = strlen (MIME_MULTIPART);
  if (strncmp (p, MIME_MULTIPART, l))
  {
    debug ("not a multipart\n");
    return (0);
  }
  if ((p = strstr (p + l + 1, "boundary=\"")) == NULL)
  {
    debug ("no boundary found\n");
    return (0);
  }
  p += 10;
  if (buf == NULL)
  {
    ch = strchr (p, '"');
    if (ch == NULL)
      return (0);
    debug ("just length for %.*s\n", ch - p, p);
    return (ch - p + 4);
  }
  /*
   * then set up to parse out multiparts
   */
  strcpy (buf, "\n--");
  for (l = 0; (buf[l + 3] = p[l]) != '\"'; l++)
  {
    if ((p[l] == 0) || (l + 4 >= sz))
      return (-1);
  }
  buf[l +=3] = 0;
  return (l);				/* boundary length	*/
}

/*
 * Return a mime part - indexed from 1
 */
MIME *mime_getMultiPart (MIME *m, int part)
{
  while ((m != NULL) && part--)
    m = m->next;
  return (m);
}

/*
 * parse a mime package
 */
MIME *mime_parse (char *buf)
{
  char *ch, *p;
  int l, c;
  MIME *m, **n;
  char boundary[100];

  /*
   * first find the header end 
   */
  if ((ch = strstr (buf, "\n\r\n")) != NULL)
    ch += 3;
  else if ((ch = strstr (buf, "\n\n")) != NULL)
    ch += 2;
  else
  {
    debug ("headers not found\n");
    return (NULL);
  }
  c = *ch;
  *ch = 0;
  m = mime_alloc ();
  m->headers = (char *) malloc (ch - buf + 1);
  strcpy (m->headers, buf);
  *ch = c;
  debug ("headers:\n%s", m->headers);
  /*
   * next determine the size 
   */
  
  if ((m->len = mime_getLength (m)) == 0)
  {
    m->len = strlen (ch);
    mime_setLength (m, m->len);
  }
  debug ("len=%d\n", m->len);
  /* 
   * copy in the body
   * if not a multipart then it's all body
   * we alloc an extra byte for the EOS
   */
  if ((l = mime_getBoundary (m, boundary, 100)) < 1)
  {
    m->body = (unsigned char *) malloc (m->len + 1);
    memmove (m->body, ch, m->len);
    m->body[m->len] = 0;
    debug ("parse completed\n");
    return (m);
  }

  /*
   * each part goes into the next chain
   */
  n = &m->next;
  p = strstr (ch - 1, boundary);
  if (p > ch -1)
  {
    m->len = p - ch;
    c = *p;
    *p = 0;
    m->body = (unsigned char *) malloc (m->len + 1);
    memmove (m->body, ch, m->len + 1);
  }
  else
    m->len = 0;
  ch = p;
  while (1)
  {
    ch += l;			/* bump past boundary marker	*/
    debug ("checking end of boundry\n");
    if ((ch[0] == ch[1]) && (ch[0] == '-'))
      break;			/* find next one		*/
    if (*ch++ == '\r') ch++;	/* bump past CR LF		*/
    if ((p = strstr (ch, boundary)) == NULL)
    {
      debug ("end boundary not found\n");
      return (mime_free (m));
    }
    c = *p;			/* parse to it			*/
    *p = 0;
    debug ("next part...\n");
    if ((*n = mime_parse (ch)) == NULL)
    {
      debug ("next part did not parse\n");
      return (mime_free (m));
    }
    n = &(*n)->next;
    *p = c;
    ch = p;			/* set up for next part		*/
  }
  debug ("parse completed\n");
  return (m);
}

/*
 * Get the size of the formatted mime message.
 * Sets the Content-Length in the header...
 */
int mime_size (MIME *m)
{
  int sz, l;
  char *b, *ch;
  MIME *part;

  debug ("sizing...\n");
  if ((m == NULL) || (m->headers == NULL))
  {
    debug ("null MIME or headers\n");
    return (0);
  }
  sz = m->len;
  if ((l = mime_getBoundary (m, NULL, 0)) > 0)
  {
    debug ("sizing parts...\n");
    for (part = m->next; part != NULL; part = part->next)
      sz += l + mime_size (part);
    sz += l + 2;
  }
  mime_setLength (m, sz);
  sz += strlen (m->headers);
  debug ("estimated mime size=%d\n", sz);
  return (sz);
}

/*
 * format a mime message - this is the recursive on, not intended
 * for external use.
 */
int mime_format_part (MIME *m, DBUF *b)
{
  MIME *n;
  char boundary[100];
  int bl;

  mime_size (m);
  dbuf_printf (b, "%s", m->headers);
  dbuf_write (b, m->body, m->len);
  if ((bl = mime_getBoundary (m, boundary, 100)) < 1)
    return (dbuf_size (b));
  boundary[bl++] = '\n';
  for (n = m->next; n != NULL; n = n->next)
  {
    debug ("adding next part at %d\n", dbuf_size (b));
    dbuf_write (b, boundary, bl);
    mime_format_part (n, b);
  }
  debug ("adding final boundary at %d\n", dbuf_size (b));
  dbuf_write (b, boundary, bl - 1);
  dbuf_write (b, "--\n", 4);
  debug ("final size is %d\n", dbuf_size (b));
  debug ("actual size is %d (%d/%d)\n",
      strlen (dbuf_getbuf(b)), strlen (m->headers), 
      strlen (dbuf_getbuf(b)) - strlen (m->headers));
  return (dbuf_size (b));
}

/*
 * Format a MIME message to an allocated character buffer.  Caller
 * is responsible for freeing this buffer.
 */
char *mime_format (MIME *m)
{
  DBUF *b;

  b = dbuf_alloc ();
  mime_format_part (m, b);
  return (dbuf_extract (b));
}


#ifdef UNITTEST

#define MNAME "examples/request.txt"

void test_multipart ()
{
  MIME *m;
  FILE *fp;
  struct stat st;
  char *buf, *fbuf;
  int sz;

  if (stat (MNAME, &st))
  {
    error ("Can't stat %s\n", MNAME);
    return;
  }
  buf = (char *) malloc (st.st_size + 1);
  fp = fopen (MNAME, "rb");
  fread (buf, 1, st.st_size, fp);
  fclose (fp);
  buf[st.st_size] = 0;
  debug ("parsing %s sz=%d...\n", MNAME, st.st_size);
  if ((m = mime_parse (buf)) == NULL)
    error ("Failed parsing %s\n", MNAME);
  fbuf = mime_format (m);
  info ("Passed multipart mime test\n", fbuf);
  debug ("formated...\n%s\n", fbuf);
  free (buf);
  free (fbuf);
}

int main (int argc, char **argv)
{
  test_multipart ();
  exit (Errors);
}

#endif /* UNITTEST */
