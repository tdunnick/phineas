/*
 * dbuf.h
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
 * dynamic buffer
 */

#ifndef __DBUF__
#define __DBUF__

#include <stdio.h>
#include <stdarg.h>

#ifndef SAFETY
#define SAFETY(c) if (c) \
  fprintf (stderr, "FATAL SAFETY at %s %d\n",__FILE__,__LINE__), exit (1)
#endif

#define DBUFSZ 1024

typedef struct dbuf
{
  int sz;
  int maxsz;
  char **index;
  unsigned char *buf;
} DBUF;


/* dynamic buffer management */
#ifdef __DBUFMAC__
#define dbuf_size(b) ((b)->sz)
#define dbuf_maxsize(b) ((b)->maxsz)
#define dbuf_getbuf(b) ((b)->buf)
#define dbuf_putp(b,p) (dbuf_write(b,&(p),sizeof(void*)))
#else
int dbuf_size (DBUF *b);
int dbuf_maxsize (DBUF *b);
unsigned char *dbuf_getbuf (DBUF *b);
DBUF *dbuf_setbuf (DBUF *b, void *buf, int sz);
unsigned char *dbuf_extract (DBUF *b);
int dbuf_putp (DBUF *b, void *p);
#endif
void *dbuf_getp  (DBUF *b, int offset);
int dbuf_setsize (DBUF *b, int sz);
void dbuf_clear (DBUF *b);
int dbuf_delete (DBUF *b, int offset, int len);
int dbuf_printf (DBUF *b, char *fmt, ...);
int dbuf_insert (DBUF *b, int offset, void *p, int sz);
int dbuf_write (DBUF *b, void *p, int sz);
int dbuf_putc (DBUF *b, int c);
int dbuf_puti (DBUF *b, int v);
unsigned char *dbuf_expand (DBUF *b, int sz);
int dbuf_parse (DBUF *b, int offset, char *match);
void *dbuf_item (DBUF *b, int offset, int item);
DBUF *dbuf_alloc (void);
void dbuf_free (DBUF *b);

#endif /* __DBUF__ */
