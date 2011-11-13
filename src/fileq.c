/*
 * fileq.c
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
#ifdef UNITTEST
#define __FILEQ__
#endif

#ifdef __FILEQ__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef UNITTEST
#undef UNITTEST
#include "unittest.h"
#include "util.c"
#include "xml.c"
#include "dbuf.c"
#include "queue.c"
#define UNITTEST
#define debug _DEBUG_
#else
#include "log.h"
#endif

#include "util.h"
#include "dbuf.h"
#include "queue.h"

#ifndef debug
#define debug(fmt...)
#endif

/* file based record are tab delimited */
#define Q_SEP '\t'

/* size of our index (for now)		*/
#define FILEQXSZ 500

typedef struct fileq
{
  struct fileq *next;			/* next queue			*/
  int rowid, 				/* max row number		*/
      offset,				/* offset to first row		*/
      sz; 				/* size of row index		*/
  FILE *fp;				/* data file			*/
  char *name;				/* queue name			*/
  long transport;			/* next transport row to pop	*/
  long row[];				/* row index			*/
} FILEQ;

FILEQ *FileQ = NULL;

/*
 * allocate a cache
 */
FILEQ *fileq_alloc (char *name, FILE *fp, int sz)
{
  int tsz;
  FILEQ *c;

  tsz = sizeof (FILEQ) + strlen (name) + 1 + sz * sizeof (long *);
  c = (FILEQ *) malloc (tsz);
  memset (c, 0, tsz); 
  c->name = (char *) (c->row + sz);
  strcpy (c->name, name);
  c->sz = sz;
  c->offset = 0;
  c->fp = fp;
  return (c);
}

/*
 * free a cache
 */
FILEQ *fileq_free (FILEQ *c)
{
  int i;

  if (c->fp != NULL)
    fclose (c->fp);
  free (c);
  return (NULL);
}


/*
 * find a cache by name
 */
FILEQ *fileq_find (QUEUE *q)
{
  FILEQ *c;
  FILE *fp;
  char path[MAX_PATH];

  for (c = FileQ; c != NULL; c = c->next)
  {
    if (strcmp (q->name, c->name) == 0)
      return (c);
  }
  pathf (path, "%s%s.txt", q->conn->conn, q->table);
  if ((fp = fopen (path, "a+")) == NULL)
  {
    error ("Can't open fileq %s\n", path);
    return (NULL);
  }
  debug ("opened file %s\n", path);
  c = fileq_alloc (q->name, fp, FILEQXSZ);
  if (fileq_reindex (q, c) < 0)
    return (fileq_free (c));
  c->next = FileQ;
  debug ("added queue last row=%d\n", c->rowid);
  return (FileQ = c);
}

/*
 * add/update an index entry
 */
int fileq_index (FILEQ *c, int r, long p)
{
  if (r - c->offset < 0)
    return (0);
  if (r - c->offset >= c->sz)
  {
    debug ("full index, moving...\n");
    int need = r - c->offset - c->sz + 1;
    memmove (c->row, c->row + need, (c->sz - need) * sizeof (long));
    memset (c->row + c->sz - need, 0, need * sizeof (long));
    c->offset += need;
  }
  c->row[r - c->offset] = p;
  if (r > c->rowid)
    c->rowid = r;
  return (r);
}

/*
 * (re)index a queue
 */
int fileq_reindex (QUEUE *q, FILEQ *c)
{
  long p;
  int i;
  char *ch, *bp, buf[QBUFSZ];

  debug ("re-indexing %s...\n", c->name);
  if (c->fp == NULL)
    return (-1);
  rewind (c->fp);
  c->rowid = 0;
  c->offset = 1;
  /*
   * If this is a new queue, initialize by inserting column names.
   * Otherwise, check to make sure column names match and reindex!
   */
  if (fgets (buf, QBUFSZ, c->fp) == NULL)
  {
    debug ("New queue... adding column names\n");
    fprintf (c->fp, "%s", q->type->field[0]);
    for (i = 1; i < q->type->numfields; i++)
      fprintf (c->fp, "%c%s", Q_SEP, q->type->field[i]);
    fputc ('\n', c->fp);
    if (queue_field_find (q, "TRANSPORTSTATUS") > 0)
      c->transport = ftell (c->fp);
  }
  else
  {
    bp = buf;
    if ((ch = strchr (buf, '\n')) != NULL)
      *ch = Q_SEP;
    else
    {
      debug ("first record missing newline\n");
      return (-1);
    }
    for (i = 0; i < q->type->numfields; i++)
    {
      if ((ch = strchr (bp, Q_SEP)) == NULL)
      {
	debug ("Missing column %d (%s)\n", i, q->type->field[i]);
	return (-1);
      }
      *ch = 0;
      if (strcmp (bp, q->type->field[i]))
      {
	debug ("Column %d (%s) didn't match %s\n", 
	  i, bp, q->type->field[i]);
	return (-1);
      }
      bp = ch + 1;
    }
    p = ftell (c->fp);
    if (queue_field_find (q, "TRANSPORTSTATUS") > 0)
      c->transport = p;
    while (fgets (buf, QBUFSZ, c->fp) != NULL)
    {
      fileq_index (c, atoi (buf), p);
      p = ftell (c->fp);
    }
  }
  debug ("index %s created to %d from %d\n", c->name, c->rowid, c->offset);
  return (c->rowid);
}

/*
 * convert a string into a row
 */
QUEUEROW *fileq_parse (QUEUE *q, char *buf)
{
  char *p, *t;
  int i;
  QUEUEROW *r;

  if ((i = atoi (buf)) < 1)
    return (NULL);
  // debug ("parsing row %d\n", i);
  p = buf;
  r = queue_row_alloc (q);
  r->rowid = i;
  for (i = 0; i < q->type->numfields; i++)
  {
    if ((t = strchr (p, Q_SEP)) != NULL)
      *t = 0;
    r->field[i] = stralloc (r->field[i], p);
    if (t == NULL)
      break;
    *t = Q_SEP;
    p = t + 1;
  }
  return (r);
}

/*
 * convert a row back into a string
 */
char *fileq_format (QUEUEROW *row, char *buf)
{
  int i;
  char *ch;

  ch = buf + sprintf (buf, "%d", row->rowid);
  for (i = 1; i < row->queue->type->numfields; i++)
  {
    if (row->field[i] == NULL)
      *ch++ = Q_SEP;
    else
      ch += sprintf (ch, "%c%s", Q_SEP, row->field[i]);
  }
  return (buf);
}

char *fileq_getrow (FILEQ *c, int row, char *buf)
{
  long p;
  int r;
  char *nl;

  r = row;
  if ((r < c->offset) || 		/* off front end	*/
      ((r -= c->offset) >= c->sz) || 	/* off back end		*/
      ((p = c->row[r]) == 0))		/* deleted		*/
    return (NULL);
  if (fseek (c->fp, p, SEEK_SET))
    return (NULL);
  if (fgets (buf, QBUFSZ, c->fp) == NULL)
    return (NULL);
  if ((nl = strchr (buf, '\n')) != NULL)
    *nl = 0;
  if (row != atoi (buf))
    error ("RowID %d does not match %.5s\n", row, buf);
  return (buf);
}

int fileq_putrow (FILEQ *c, char *buf)
{
  int r;
  long p;

  r = atoi (buf);
  if (fseek (c->fp, 0, SEEK_END))
    return (0);
  p = ftell (c->fp);
  fputs (buf, c->fp);
  fputc ('\n', c->fp);
  fileq_index (c, r, p);
  return (r);
}

/*
 * read to the next transport record to pop
 */
QUEUEROW *fileq_transport (QUEUE *q, FILEQ *c)
{
  int i, f, r;
  long p;
  char *ch, buf[QBUFSZ];

  if (fseek (c->fp, c->transport, SEEK_SET))
    return (NULL);
  if ((f = queue_field_find (q, "TRANSPORTSTATUS")) < 0)
    return (NULL);
  while (fgets (buf, QBUFSZ, c->fp) != NULL)
  {
    p = c->transport;
    c->transport = ftell (c->fp);
    ch = buf;
    for (i = 0; i < f; i++)
    {
      if ((ch = strchr (ch, Q_SEP)) == NULL)
	break;
      ch++;
    }
    // debug ("i=%d/%d ch=%.8s\n", i, f, ch == NULL ? "NULL" : ch);
    if ((ch != NULL) && strstarts (ch, "queued"))
    {
      r = atoi (buf);			/* check if current	*/
      if ((r < c->offset) || 		/* off front end	*/
        ((r -= c->offset) > c->sz) || 	/* off back end		*/
        (p == c->row[r]))		/* this row!		*/
        return (fileq_parse (q, buf));
    }
  }
  return (NULL);
}

/********************* advertized functions ***********************/

/*
 * shutdown file based queuing
 */
int fileq_close (void *conn)
{
  FILEQ *c;

  while (FileQ != NULL)
  {
    c = FileQ;
    FileQ = c->next;
    fileq_free (c);
  }
  return (0);
}

/*
 * add a row to the queue
 */
int fileq_push (QUEUEROW *r)
{
  FILEQ *c;
  char buf[QBUFSZ];

  if ((c = fileq_find (r->queue)) == NULL)
    return (-1);
  if (r->rowid == 0)
    r->rowid = ++c->rowid;
  return (fileq_putrow (c, fileq_format (r, buf)));
}

/*
 * get a row
 */
QUEUEROW *fileq_get (QUEUE *q, int rowid)
{
  FILEQ *c;
  char buf[QBUFSZ];

  if ((c = fileq_find (q)) == NULL)
    return (NULL);
  return (fileq_parse (q, fileq_getrow (c, rowid, buf)));
}

/*
 * get the next row
 */
QUEUEROW *fileq_next (QUEUE *q, int rowid)
{
  FILEQ *c;
  char buf[QBUFSZ];

  if ((c = fileq_find (q)) == NULL)
    return (NULL);
  /*
   * call with 0 to get the first row
   */
  if (rowid == 0)
  {
    rowid = c->offset;
  }
  else if (rowid < c->offset)
  {
    return (NULL);
  }
  while (++rowid <= c->rowid)
  {
    if (c->row[rowid - c->offset])
    {
      return (fileq_parse (q, fileq_getrow (c, rowid, buf)));
    }
  }
  return (NULL);
}

/*
 * get the previous row
 */
QUEUEROW *fileq_prev (QUEUE *q, int rowid)
{
  FILEQ *c;
  char buf[QBUFSZ];

  if ((c = fileq_find (q)) == NULL)
    return (NULL);
  /*
   * call with 0 to get last row
   */
  if (rowid == 0)
  {
    rowid = c->rowid + 1;
  }
  else if (rowid > c->rowid + 1)
  {
    return (NULL);
  }
  while (--rowid >= c->offset)
  {
    if (c->row[rowid - c->offset])
    {
      return (fileq_parse (q, fileq_getrow (c, rowid, buf)));
    }
  }
  return (NULL);
}

/*
 * return the top row, removing it from our index
 */
QUEUEROW *fileq_pop (QUEUE *q)
{
  FILEQ *c;
  QUEUEROW *r;

  if ((c = fileq_find (q)) == NULL)
    return (NULL);
  if (c->transport)
    r = fileq_transport (q, c);
  else 
    r = fileq_prev (q, c->rowid + 1);
  if (r == NULL)
    return (NULL);
  if (r->rowid >= c->offset)
    c->row[r->rowid - c->offset] = 0;
  return (r);
}

/*
 * Making a connection...
 * note our private connection object is the unc path (prefix) to the folder
 * with the database file
 */
int fileq_connect (QUEUECONN *conn)
{
  conn->conn = conn->unc;
  conn->close = fileq_close;
  conn->push = fileq_push;
  conn->pop = fileq_pop;
  conn->get = fileq_get;
  conn->nextrow = fileq_next;
  conn->prevrow = fileq_prev;
  debug ("Connection to %s completed\n", conn->name);
  return (0);
}

#ifdef UNITTEST

char TestRow[] =
"99\tmessage ID\tpayload name\tlocal name\tservice\taction\t"
"some arguments\tfrom party\tthe recipient\tan error code\t"
"an error message\tprocessing status\tapplication status\t"
"was encrypted\twhen received\tlast updated\tprocess flag";

char *rand_rcv (int rowid, char *buf)
{
  char *names[] = { "data", "report", "out", "file", "dump" };
  char *service[] = { "elr", "hiv", "influenza" };
  char *action[] = { "save", "update", "forward", "notify" };
  char *emsg[] = { "none", "failed", "", "" };
  char *status[] = { "done", "failed", "queued", "in process" };
  char *statusa[] = { "ok", "failed", "waiting", "processing" };
  char *args[] = { "do=this", "don't=that", "config=none", "" };
  char *from[] = { "foo.com", "bar.state.edu", "some.lab.net", "joes.biz" };
  char *pname = names[rand()%5];
  char pbuf[PTIMESZ], pbuf2[PTIMESZ], pid[PTIMESZ];
  int ecode = rand()%4;
  time_t t, t2; time (&t); t -= rand()%20000; t2 = t + rand()%10000;

  ptime (&t, pbuf);
  ptime (&t2, pbuf2);

  sprintf (buf, "%d\tmessage_%d\t%s\tlocal_%s\t%s\t%s\t"
      "%s\t%s\tlocalhost\t%d\t"
      "%s\t%s\t%s\t"
      "%s\t%s\t%s\t%s",
      rowid,rand(),pname,pname,service[rand()%3],action[rand()%4],
      args[rand()%4],from[rand()%4],ecode,
      emsg[ecode],status[ecode],statusa[ecode],
      rand()%1?"yes":"no",pbuf,pbuf2,ppid (pid)
      );
  return (buf);
}

char *rand_trans (int rowid, char *buf)
{
  char *names[] = { "data", "report", "out", "file", "dump" };
  char *service[] = { "elr", "hiv", "influenza" };
  char *action[] = { "save", "update", "forward", "notify" };
  char *emsg[] = { "none", "failed", "", "" };
  char *status[] = { "done", "failed", "queued", "in process" };
  char *statusa[] = { "ok", "failed", "waiting", "processing" };
  char *args[] = { "do=this", "don't=that", "config=none", "" };
  char *from[] = { "foo.com", "bar.state.edu", "some.lab.net", "joes.biz" };
  char *pname = names[rand()%5];
  char *fromdn = from[rand()%4];
  char pbuf[PTIMESZ], pbuf2[PTIMESZ];
  int ecode = rand()%4;
  time_t t, t2; time (&t); t -= rand()%20000; t2 = t + rand()%10000;

  ptime (&t, pbuf);
  ptime (&t2, pbuf2);
  sprintf (buf, 
      "%d\tmessage_%d\t%s\tlocal_%s\troute_%d\t%s\t%s\t"
      "%s\t%s\t%s\t%s\t"
      "%s\t%s\t%s\tDN=%s\tURL=%s\t"
      "%s\t%s\t%d\t"
      "%s\t%d\t%s\t"
      "%s\t%s\tresponse_%d\t"
      "%s\trlocal_%s\t%s\t%s\t%s\t%d",
      rowid,rand(),pname,pname,rand(),service[rand()%3],action[rand()%4],
      args[rand()%4],fromdn,pbuf2,rand()%1?"yes":"no",
      "","",fromdn,"",fromdn,
      statusa[ecode],status[ecode],ecode,
      status[ecode],ecode,emsg[ecode],
      pbuf, pbuf2, rand(),
      args[rand()%4],pname,pname,fromdn,"",rand()%4
      );
  return (buf);
}

int init_queue (QUEUE *q, char *(*fn)(), int numrows, char *buf)
{
  int i, f, n, v, l;
  FILE *fp;

  pathf (buf, "%s%s.txt", q->conn->conn, q->table);
  fp = fopen (buf, "w");
  fprintf (fp, "%s", q->type->field[0]);
  for (i = 1; i < q->type->numfields; i++)
    fprintf (fp, "%c%s", Q_SEP, q->type->field[i]);
  fputc ('\n', fp);
  for (i = 0; i < numrows; i++)
  {
    fprintf (fp, "%s\n", fn (i + 1, buf));
  }
  fclose (fp);
}

int build_queues (char *buf)
{
  QUEUE *q;
  QUEUEROW *r;

  if ((q = queue_find ("MemReceiveQ")) == NULL)
    fatal ("Couldn't find MemReceiveQ\n");
  if ((r = fileq_parse (q, rand_rcv (1, buf))) == NULL)
    fatal ("random parse failed for %s\n", q->name);
  free (r);
  init_queue (q, rand_rcv, 100, buf);
  if ((q = queue_find ("MemSendQ")) == NULL)
    fatal ("Couldn't find MemSendQ\n");
  if ((r = fileq_parse (q, rand_trans (1, buf))) == NULL)
    fatal ("random parse failed for %s\n", q->name);
  free (r);
  init_queue (q, rand_trans, 100, buf);
  return (0);
}

int dump_row (QUEUEROW *r)
{
  if (r == NULL)
    return;
  debug ("Row %d %s %s\n", r->rowid, queue_field_get (r, "MESSAGEID"),
    queue_field_get (r, "TRANSPORTSTATUS"));
  free (r);
}

int main (int argc, char **argv)
{
  int i;
  XML *xml;
  QUEUE *q;
  QUEUETYPE *t;
  QUEUEROW *r = NULL;
  char *ch = NULL;
  char buf[4096];

  if ((xml = xml_parse (PhineasConfig)) == NULL)
    return (-1);
  loadpath (xml_get_text (xml, "Phineas.InstallDirectory"));
  xml_set_text (xml, "Phineas.QueueInfo.Queue[0].Table", "TransportQ.test");
  xml_set_text (xml, "Phineas.QueueInfo.Queue[1].Table", "ReceiveQ.test");
  debug ("Configuring...\n");
  if (queue_init (xml))
    fatal ("Couldn't initialize\n"); 
  if (argc > 1)
    build_queues (buf);
  debug ("MemReceiveQ tests...\n");
  q = queue_find ("MemReceiveQ");
  if (q == NULL)
    error ("Couldn't find MemReceiveQ\n");
  else if ((r = fileq_parse (q, TestRow)) == NULL)
    error ("Couln't parse test row\n");
  else if (fileq_format (r, buf) == NULL)
    error ("Couldn't format test row\n");
  else if (strcmp (buf, TestRow))
    error ("Test row didn't match formatting\n%s\n", buf);
  if (r != NULL)
    queue_row_free (r);
  if (fileq_find (q) == NULL)
    error ("Failed finding queue %s\n", q->name);
  debug ("Popping...\n");
  if ((r = queue_pop (q)) == NULL)
    error ("Couldn't pop queue %s\n", q->name);
  else
  {
    i = r->rowid - 17;
    dump_row (r);
  }
  debug ("Getting...\n");
  if ((r = queue_get (q, i)) == NULL)
    error ("Couldn't get row %d queue %s\n", i, q->name);
  else
    dump_row (r);
  debug ("Prev...\n");
  if ((r = queue_prev (q, i)) == NULL)
    error ("Couldn't get prev row %d queue %s\n", i, q->name);
  else
    dump_row (r);
  debug ("Next...\n");
  while ((r = queue_next (q, i)) != NULL)
  {
    i = r->rowid;
    dump_row (r);
  }
  debug ("default prev...\n");
  if ((r = queue_prev (q, 0)) == NULL)
    error ("Couldn't get default prev queue %s\n", q->name);
  else
    dump_row (r);

  debug ("MemSendQ tests...\n");
  q = queue_find ("MemSendQ");
  if (q == NULL)
    error ("Couldn't find MemSendQ\n");
  debug ("Popping...\n");
  if ((r = queue_pop (q)) == NULL)
    error ("Couldn't pop queue %s\n", q->name);
  else
    dump_row (r);
  while ((r = queue_pop (q)) != NULL)
  {
    i = r->rowid;
    dump_row (r);
  }
  debug ("Prev...\n");
  if ((r = queue_prev (q, i)) == NULL)
    error ("Couldn't get prev row %d queue %s\n", i, q->name);
  else
    dump_row (r);
  debug ("Default Prev/Next\n");
  if ((r = queue_prev (q, 0)) == NULL)
    error ("Couldn't get default prev row from queue %s\n", q->name);
  else 
    dump_row (r);
  if ((r = queue_next (q, 0)) == NULL)
    error ("Couldn't get default next row from queue %s\n", q->name);
  else
    dump_row (r);
  debug ("Closing...\n");
  queue_shutdown ();
  info ("%s unit test completed!\n", argv[0]);
}

#endif /* UNITTEST */
#endif /* __FILEQ__ */
