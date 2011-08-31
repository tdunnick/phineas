/*
 * dbuf.c
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
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "dbuf.h"

/*
 * Dynamic buffers
 *
 * To adjust size work something like files for output, but
 * for efficiency are useds as character buffers for input.
 *
 * Note many of these may be implemented as macros...
 */

/* return current buffer used */
#ifndef dbuf_size
int dbuf_size (DBUF *b)
{
  SAFETY (b == NULL);
  return (b->sz);
}
#endif

/* reset buffer size */
int dbuf_setsize (DBUF *b, int sz)
{
  SAFETY (b == NULL);
  if (sz > 0)
  {
    if (sz >= b->maxsz)
      dbuf_expand (b, sz - b->sz);
    else if (sz < b->sz)
      memset (b->buf + sz, 0, b->sz - sz);
  }
  return (b->sz = sz);
}

/* return buffer capacity */
#ifndef dbuf_maxsize
int dbuf_maxsize (DBUF *b)
{
  SAFETY (b == NULL);
  return (b->maxsz);
}
#endif

/* return pointer to buffer contents */
#ifndef dbuf_getbuf
unsigned char *dbuf_getbuf (DBUF *b)
{
  SAFETY (b == NULL);
  return (b->buf);
}
#endif

/* set a dynamic buffer */
DBUF *dbuf_setbuf (DBUF *b, void *buf, int sz)
{
  if (b == NULL)
    b = (DBUF *) malloc (sizeof (DBUF));
  else if (b->buf != NULL)
    free (b->buf);
  b->sz = b->maxsz = sz;
  b->buf = buf;
  return (b);
}

/* clears the buffer contents and reset use count */
void dbuf_clear (DBUF *b)
{
  SAFETY (b == NULL);
  memset (b->buf, 0, b->sz);
  b->sz = 0;
}

/*
 * delete data from the buf
 */
int dbuf_delete (DBUF *b, int offset, int len)
{
  if (offset + len >= b->sz)
    return (-1);
  memmove (b->buf + offset, b->buf + offset + len, b->sz - (offset + len));
  return (b->sz -= len);
}

/* formatted append to buffer returning offset in buffer
 *
 * note documentation for vsnprintf suggests it is supposed to return
 * amount that WOULD BE used for too small a buffer... here it seems
 * to return -1 (sigh).
 */
int dbuf_printf (DBUF *b, char *fmt, ...)
{
  int used, sz;
  va_list ap;

  SAFETY ((b == NULL) || (fmt == NULL));
  sz = b->sz;
  va_start (ap, fmt);
  used = vsprintf (NULL, fmt, ap);
  va_end (ap);
  if (used <= 0)
    return (-1);
  dbuf_expand (b, used + 1);
  b->sz += used;
  va_start (ap, fmt);
  vsprintf (b->buf + sz, fmt, ap);
  va_end (ap);
  return (sz);
}

/* insert data into a buffer */
int dbuf_insert (DBUF *b, int offset, void *p, int sz)
{
  int msz;

  SAFETY (b == NULL);
  if ((msz = b->sz - offset) <= 0)
    return (dbuf_write (b, p, sz));
  dbuf_expand (b, sz);
  memmove (b->buf + offset + sz, b->buf + offset, msz);
  memcpy (b->buf + offset, p, sz);
  b->sz += sz;
  return (offset);
}

/* arbitrary write to buffer */
int dbuf_write (DBUF *b, void *p, int sz)
{
  int offset;
  SAFETY (b == NULL);
  offset = b->sz;
  dbuf_expand (b, sz);
  memcpy (b->buf + b->sz, p, sz);
  b->sz += sz;
  return (offset);
}

/* character write to buffer */
int dbuf_putc (DBUF *b, int c)
{
  SAFETY (b == NULL);
  dbuf_expand (b, 1);
  b->buf[b->sz++] = c;
  return (b->sz - 1);
}

/* integer write */
int dbuf_puti (DBUF *b, int c)
{
  return (dbuf_write (b, &c, sizeof (int)));
}

/* pointer write */
#ifndef dbuf_putp
int dbuf_putp (DBUF *b, void *p)
{
  return (dbuf_write (b, &p, sizeof (void *)));
}
#endif

/* expand buffer capacity to allow for additional size
 * return the new buffer location
 */

unsigned char *dbuf_expand (DBUF *b, int sz)
{
  int newsz = b->maxsz;

  SAFETY (b == NULL);
  while (b->sz + sz >= newsz)
    newsz <<= 1;
  if (newsz > b->maxsz)
  {
    b->buf = (unsigned char *) realloc (b->buf, newsz);
    SAFETY (b->buf == NULL);
    memset (b->buf + b->maxsz, 0, newsz - b->maxsz);
    b->maxsz = newsz;
  }
  return (b->buf);
}

/*
 * Parse strings in the buffer starting at offset up to an EOS
 * and delimited by the match string. Build an array of offsets
 * and return it.  Used dbuf_get() to retrieve the strings.
 */
int dbuf_parse (DBUF *b, int offset, char *match)
{
  int i, n, l, index;
  unsigned char *ch;

  SAFETY ((b == NULL) || (match == NULL));
  n = 1;				/* count needed entries	*/
  ch = b->buf + offset;
  l = strlen (match);
  for (i = 0; (i < b->sz) && ch[i]; i++)
  {
    if (strncmp (ch + i, match, l) == 0)
      n++;
  }
  if (i == b->sz)
    dbuf_putc (b, 0);				/* safety EOS		*/
  index = b->sz;				/* our index start	*/
  ch = dbuf_expand (b, n * sizeof (int));
  dbuf_puti (b, offset);
  while (ch[offset])
  {
    if (strncmp (ch + offset, match, l) == 0)
    {
      ch[offset] = 0;
      offset += l;
      dbuf_puti (b, offset);
    }
    else
      offset++;
  }
  dbuf_puti (b, -1);
  return (index);
}

/*
 * return a pointer to this offset in the buffer
 * or NULL if not valid offset
 */
void *dbuf_getp  (DBUF *b, int offset)
{
  if ((offset < 0) || (offset >= b->sz))
    return (NULL);
  return ((void *) (b->buf + offset));
}

/*
 * return the pointer value (indirection) at this offset in the buffer
 * or NULL if not valid offset
 */
void *dbuf_item (DBUF *b, int offset, int item)
{
  int *l;

  if ((l = dbuf_getp (b, offset)) != NULL)
    return (dbuf_getp (b, l[item]));
  return (NULL);
}

/* allocate and initialize a buffer to default capacity */
DBUF *dbuf_alloc (void)
{
  DBUF *b = (DBUF *) malloc (sizeof (DBUF));
  b->sz = 0;
  b->maxsz = DBUFSZ;
  b->buf = (unsigned char *) malloc (b->maxsz);
  memset (b->buf, 0, b->maxsz);
  return (b);
}

/* free a buffer */
void dbuf_free (DBUF *b)
{
  if (b == NULL)
    return;
  if (b->buf != NULL)
    free (b->buf);
  free (b);
}

/* extract the buf and free the structure */
unsigned char *dbuf_extract (DBUF *b)
{
  char *ch;

  SAFETY ((b == NULL) || (b->sz == b->maxsz));
  b->buf[b->sz] = 0;			/* force EOS termination	*/
  ch = b->buf;
  free (b);
  return (ch);
}
