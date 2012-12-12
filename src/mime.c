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

/*
 * Rfc2046 expects line boundaries to consist of CRLF pairs.
 * Here that is slightly eased during parsing to only require LF. 
 * That may introduce other bugs for bogus messages.
 */

#ifdef UNITTEST
#include "unittest.h"
#endif

#include <stdlib.h>
#include <sys/stat.h>

#include "log.h"
#include "mime.h"
#include "dbuf.h"

#ifndef debug
#define debug(fmt...)
#endif

int mime_format_part (MIME *mime, DBUF *b);

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

  // debug ("old headers...\n%s", m->headers);
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
    // debug ("removed %s resuling header...\n%s", name, m->headers);
  }
  // debug ("realloc...\n%s", m->headers);
  m->headers = (char *) realloc (m->headers, sz + strlen (m->headers) + 1);
  // debug ("after...\n%s", m->headers);
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
  // debug ("new headers...\n%s", m->headers);
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
  debug ("header %s:%.*s\n", name, strchr(ch,'\r') - ch, ch);
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
  ch = strchr (p, '"');
  if (ch == NULL) 
    return (-1);
  if (buf != NULL)
  {
    if ((ch - p) + 2 >= sz)
      return (-1);
    sprintf (buf, "--%.*s", ch - p, p);
  }
  return (ch - p + 2);
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
  // debug ("headers:\n%s", m->headers);
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
  *boundary = '\n';
  if ((l = mime_getBoundary (m, boundary + 1, 99) + 1) < 2)
  {
    m->body = (unsigned char *) malloc (m->len + 1);
    memmove (m->body, ch, m->len);
    m->body[m->len] = 0;
    debug ("parse completed\n");
    return (m);
  }

  /*
   * each part goes into the next chain
   * at this point ch is at the end of the header, so we look for
   * the first boundary, which may be where we are at.  Note boundry
   * start with a newline, so backup one for the check in case there
   * is no body.
   *
   */
  n = &m->next;
  p = strstr (ch - 1, boundary);
  if (p > ch - 1)
  {
    m->len = p - ch;
    c = *p;
    *p = 0;
    m->body = (unsigned char *) malloc (m->len + 1);
    memmove (m->body, ch, m->len + 1);
  }
  else
    m->len = 0;
  ch = p;			/* start at boundry and...	*/
  while (1)			/* collect next part		*/
  {
    ch += l;			/* bump past boundary marker	*/
    debug ("checking end of boundry\n");
    if ((ch[0] == ch[1]) && (ch[0] == '-'))
      break;
    				/* find next one		*/
    if (*ch++ == '\r') ch++;	/* bump past CR   		*/
    if ((p = strstr (ch, boundary)) == NULL)
    {
      debug ("end boundary not found at...\n%s\n", ch);
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

  if ((m == NULL) || (m->headers == NULL))
  {
    debug ("null MIME or headers\n");
    return (0);
  }
  sz = m->len;
  if ((l = mime_getBoundary (m, NULL, 0)) > 0)
  {
    debug ("sizing parts...\n");
    l += 4;		/* each boundry surround by CRLF	*/
    for (part = m->next; part != NULL; part = part->next)
      sz += l + mime_size (part);
    sz += l + 2;	/* last boundry has "--" added		*/
  }
  mime_setLength (m, sz);
  debug ("mime headers=%d body=%d size=%d\n", 
      strlen (m->headers), sz, sz + strlen (m->headers));
  return (sz + strlen (m->headers));
}

/*
 * format a mime message - this is the recursive one, not intended
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
  // prefix boundary with CR
  boundary[0] = '\r';
  boundary[1] = '\n';
  if ((bl = mime_getBoundary (m, boundary + 2, 98) + 2) < 3)
    return (dbuf_size (b));
  boundary[bl++] = '\r';
  boundary[bl++] = '\n';
  for (n = m->next; n != NULL; n = n->next)
  {
    debug ("adding next part at %d\n", dbuf_size (b));
    dbuf_write (b, boundary, bl);
    mime_format_part (n, b);
  }
  debug ("adding final boundary at %d\n", dbuf_size (b));
  dbuf_write (b, boundary, bl - 2);
  dbuf_write (b, "--\r\n", 4);
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
#undef UNITTEST
#undef debug
#include "dbuf.c"

#define MNAME "../examples/request.txt"
char *SimpleTest =
"Host: slhw0008.ad.slh.wisc.edu:5088\r\n"
"Connection: Keep-Alive\r\n"
"Content-Type: \"text/plain\"\r\n"
"SOAPAction: \"ebXML\"\r\n"
"Content-Length: 47\r\n"
"\r\n"
"the quick brown fox jumped over the lazy dogs\r\n";

int test_size (char *name, char *msg)
{
  int len = 0;
  char *ch = strstr (msg, MIME_LENGTH);
  if (ch == NULL)
    return (0);
  len = atoi (ch + strlen (MIME_LENGTH) + 1);
  if (len < 1)
  {
    error ("%s - Bad length at %.*s\n", name, strchr(ch,'\r')-ch, ch);
    return (1);
  }
  if ((ch = strstr (ch, "\r\n\r\n")) != NULL)
    ch += 4;
  else if ((ch = strstr (ch, "\n\n")) != NULL)
    ch += 2;
  else 
  {
    error ("%s - missing end of header!", name);
    return (1);
  }
  debug ("%s size=%d hdr size=%d length=%d\n",
      name, strlen (msg), ch - msg, len);
  if (strlen (msg) != ch - msg + len)
  {
    error ("%s- Incorrect size got %d expected %d\n",
      name, len, strlen (msg) - (ch - msg));
    return (1);
  }
}

void test_buf (char *name, char *buf)
{
  MIME *m;
  char *fbuf;
  char n[1024];

  debug ("parsing %s sz=%d...\n", name, strlen (buf));
  // test_size (name, buf);  /* we know it's bad!!		*/
  if ((m = mime_parse (buf)) == NULL)
    error ("Failed parsing %s\n", name);
  fbuf = mime_format (m);
  debug ("formated...\n%s\n", fbuf);
  sprintf (n, "%s formated", name);
  test_size (n, fbuf);
  free (fbuf);
  mime_free (m);
}

void test_multipart ()
{
  FILE *fp;
  struct stat st;
  char *buf;
  int sz;

  test_buf ("simple", SimpleTest);
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
  test_buf (MNAME, buf);
  free (buf);
}

int main (int argc, char **argv)
{
  test_multipart ();
  info ("%s %s\n", argv[0], Errors?"failed":"passed");
  exit (Errors);
}

#endif /* UNITTEST */
