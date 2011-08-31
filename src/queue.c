/*
 * queue.c
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
/*
 * PHINMS Queues...
 */


#ifdef UNITTEST
#undef UNITTEST
#include "unittest.h"
#include "util.c"
#include "dbuf.c"
#include "xml.c"
#include "fileq.c"
#define UNITTEST
#define debug _DEBUG_
#else
#include "log.h"
#endif

#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "dbuf.h"
#include "queue.h"

#ifndef debug(fmt...)
#define debug(fmt...)
#endif

/*
 * connection registration list
 */
typedef struct queueapi
{
  struct queueapi *next;
  int (*connect) (QUEUECONN *conn);
  char name[1];
} QUEUEAPI;

QUEUEAPI *QApi = NULL;

/*
 * common prefixes for the XML
 */

#define QP_CONN "Phineas.QueueInfo.Connection"
#define QP_TYPE "Phineas.QueueInfo.Type"
#define QP_QUEUE "Phineas.QueueInfo.Queue"

#define Q_SEP '\t'

QUEUECONN *QConn = NULL;		/* the connections		*/
QUEUETYPE *QType = NULL;		/* the queue types		*/
QUEUE *Queue = NULL;			/* all the queues		*/

/******************** private entry points ******************************/
/*
 * allocate and open a connection from the XML configuration
 */
QUEUECONN *queue_conn_alloc (XML *xml, int index)
{
  QUEUECONN *conn;
  char *name, 
       *unc,
       *user,
       *pass,
       path[80];
  int i, sz;

  sprintf (path, "%s[%d].Name", QP_CONN, index);
  name = xml_get_text (xml, path);
  debug ("getting API for %s\n", name);
  sprintf (path, "%s[%d].Id", QP_CONN, index);
  user = xml_get_text (xml, path);
  sprintf (path, "%s[%d].Password", QP_CONN, index);
  pass = xml_get_text (xml, path);
  sprintf (path, "%s[%d].Unc", QP_CONN, index);
  unc = xml_get_text (xml, path);
  /*
   * calculate room needed
   */
  sz = sizeof (QUEUECONN) + strlen (name) + strlen (unc) + strlen (user)
    + strlen (pass) + 4;
  QUEUECONN *conn = (QUEUECONN *) malloc (sz);
  memset (conn, 0, sz);
  /*
   * copy in strings
   */
  conn->name = (char *) (conn + 1);
  strcpy (conn->name, name);
  conn->unc = conn->name + strlen (name) + 1;
  strcpy (conn->unc, unc);
  conn->user = conn->unc + strlen (unc) + 1;
  strcpy (conn->user, user);
  conn->passwd = conn->user + strlen (user) + 1;
  strcpy (conn->passwd, pass);
  return (conn);
}

/*
 * allocate a queue type from the XML configuration
 */
QUEUETYPE *queue_type_alloc (XML *xml, int index)
{
  QUEUETYPE *t;
  char *name, *ch, path[80];
  int i, fl, sz;
  int numfields;

  sprintf (path, "%s[%d].Name", QP_TYPE, index);
  debug ("allocating type %s\n", path);
  name = xml_get_text (xml, path);
  fl = sprintf (path, "%s[%d].Field", QP_TYPE, index);
  numfields = xml_count (xml, path);
  sz = 0;
  debug ("%d fields\n", numfields);
  for (i = 0; i < numfields; i++)
  {
    sprintf (path + fl, "[%d]", i);
    sz += strlen (xml_get_text (xml, path)) + 1;
  }
  t = (QUEUETYPE *) malloc (sizeof (QUEUETYPE) + strlen (name) + 1 +
    sizeof (char *) * numfields + sz);
  t->next = NULL;
  t->numfields = numfields;
  t->name = (char *) (t->field + numfields);
  strcpy (t->name, name);
  ch = t->name + strlen (name) + 1;
  for (i = 0; i < numfields; i++)
  {
    sprintf (path + fl, "[%d]", i);
    strcpy (ch, xml_get_text (xml, path));
    t->field[i] = ch;
    ch += strlen (ch) + 1;
  }
  return (t);
}

/*
 * allocate and initialize a queue from the XML
 */
QUEUE *queue_alloc (XML *xml, int index)
{
  int pl;
  QUEUETYPE *t;
  QUEUECONN *c;
  QUEUE *q;
  char *name,
       *type,
       *connection,
       *table,
       path[80];

  pl = sprintf (path, "%s[%d].", QP_QUEUE, index);
  debug ("allocating queue %s\n", path);
  strcpy (path + pl, "Name");
  name = xml_get_text (xml, path);
  strcpy (path + pl, "Type");
  type = xml_get_text (xml, path);
  for (t = QType; t != NULL; t = t->next)
  {
    if (strcmp (type, t->name) == 0)
      break;
  }
  if (t == NULL)
  {
    error ("No queue type %s found for queue %s\n", type, name);
    return (NULL);
  }
  strcpy (path + pl, "Connection");
  connection = xml_get_text (xml, path);
  for (c = QConn; c != NULL; c = c->next)
  {
    if (strcmp (connection, c->name) == 0)
      break;
  }
  if (c == NULL)
  {
    error ("No queue connection %s found for queue %s\n", connection, name);
    return (NULL);
  }
  strcpy (path + pl, "Table");
  table = xml_get_text (xml, path);
  q = (QUEUE *) malloc (sizeof (QUEUE) + strlen (name) + 1 +
    + strlen (table) + 1);
  q->next = NULL;
  init_mutex (q);
  q->type = t;
  q->conn = c;
  q->name = (char *) (q + 1);
  strcpy (q->name, name);
  q->table = q->name + strlen (name) + 1;
  strcpy (q->table, table);
  debug ("Allocated Queue %s\n", name);
  return (q);
}


/*
 * free up one queue
 */
QUEUE *queue_free (QUEUE *q)
{
  int i;

  if (q == NULL)
    return (NULL);
  wait_mutex (q);
  destroy_mutex (q);
  free (q);
  return (NULL);
}

/*
 * make a queue connection
 */
int queue_connect (QUEUECONN *conn)
{
  QUEUEAPI *p;

  if (conn->conn != NULL)
    return (0);
  for (p = QApi; p != NULL; p = p->next)
  {
    if (strcmp (p->name, conn->name) == 0)
    {
      return (p->connect (conn));
    }
  }
  return (-1);
}

/******************* advertized entry points *********************/
/*
 * Register connections
 */
int queue_register (char *name, int (*c)(QUEUECONN *))
{
  QUEUEAPI *n, **p;

  n = (QUEUEAPI *) malloc (sizeof (QUEUEAPI) + strlen (name));
  n->next = NULL;
  n->connect = c;
  strcpy (n->name, name);
  for (p = &QApi; *p != NULL; p = &(*p)->next);
  *p = n;
  return (0);
}

/*
 * Initialize queuing from a configuration file.
 * Return non-zero on error.
 */
int queue_init (XML *xml)
{
  int i, n;
  QUEUECONN *c;
  QUEUETYPE *t;
  QUEUE *q;

  if (xml == NULL)
    return (-1);
  n = xml_count (xml, QP_CONN);
  debug ("allocating %d connections\n", n);
  for (i = 0; i < n; i++)
  {
    if ((c = queue_conn_alloc (xml, i)) != NULL)
    {
      c->next = QConn;
      QConn = c;
    }
  }
  n = xml_count (xml, QP_TYPE);
  debug ("allocating %d types\n", n);
  for (i = 0; i < n; i++)
  {
    if ((t = queue_type_alloc (xml, i)) != NULL)
    {
      t->next = QType;
      QType = t;
    }
  }
  n = xml_count (xml, QP_QUEUE);
  debug ("allocating %d queues\n", n);
  for (i = 0; i < n; i++)
  {
    if ((q = queue_alloc (xml, i)) != NULL)
    {
      q->next = Queue;
      Queue = q;
    }
  }
  return (Queue == NULL ? -1 : 0);
}

/*
 * Shut down the queues and free them up.  Note you must re-register
 * the connections after a shutdown.
 */
void queue_shutdown (void)
{
  QUEUE *q;
  QUEUETYPE *t;
  QUEUECONN *c;
  QUEUEAPI *p;
  
  while (Queue != NULL)
  {
    q = Queue;
    Queue = q->next;
    queue_free (q);
  }
  while (QType != NULL)
  {
    t = QType;
    QType = t->next;
    free (t);
  }
  while (QConn != NULL)
  {
    c = QConn;
    QConn = c->next;
    if (c->conn != NULL)
      c->close (c->conn);
    free (c);
  }
  while (QApi != NULL)
  {
    p = QApi;
    QApi = p->next;
    free (p);
  }
}

/*
 * find the queue with this name
 */
QUEUE *queue_find (char *name)
{
  QUEUE *q;

  if (name == NULL)
    return (NULL);
  for (q = Queue; q != NULL; q = q->next)
  {
    if (strcmp (name, q->name) == 0)
    {
      if ((q->conn->conn == NULL) && queue_connect (q->conn))
	break;
      return (q);
    }
  }
  return (NULL);
}

/*
 * API callbacks...
 * All rows returned must be submitted to queue_row_free() for disposal.
 */

/*
 * The convensions for "pop" is to return either the 
 * highest record, or for queues with TRANSPORTSTATUS return
 * the oldest record where TRANSPORTSTATUS is "queued".
 */
QUEUEROW *queue_pop (QUEUE *q)
{
  QUEUEROW *r;

  if (q == NULL)
    return (NULL);
  wait_mutex (q);
  r = q->conn->pop (q);
  end_mutex (q);
  return (r);
}

/*
 * A "push" will update records with rowid > 0, or assign the
 * next row to this record and insert it.
 */
int queue_push (QUEUEROW *r)
{
  int id;

  if (r == NULL)
    return (-1);
  wait_mutex (r->queue);
  id = r->queue->conn->push (r);
  end_mutex (r->queue);
  return (id);
}

/*
 * Retrieve a row
 */
QUEUEROW *queue_get (QUEUE *q, int rowid)
{
  QUEUEROW *r;

  if (q == NULL)
    return (NULL);
  wait_mutex (q);
  r = q->conn->get (q, rowid);
  end_mutex (q);
  return (r);
}

/*
 * Retrieve the row after this one
 */
QUEUEROW *queue_next (QUEUE *q, int rowid)
{
  QUEUEROW *r;

  if (q == NULL)
    return (NULL);
  wait_mutex (q);
  r = q->conn->nextrow (q, rowid);
  end_mutex (q);
  return (r);
}

/*
 * Retrieve the row before this one
 */
QUEUEROW *queue_prev (QUEUE *q, int rowid)
{
  QUEUEROW *r;

  if (q == NULL)
    return (NULL);
  wait_mutex (q);
  r = q->conn->prevrow (q, rowid);
  end_mutex (q);
  return (r);
}

/*
 * Allocate a row
 */
QUEUEROW *queue_row_alloc (QUEUE *q)
{
  int sz, i;
  QUEUEROW *r;

  if (q == NULL)
    return (NULL);
  sz = sizeof (QUEUEROW) + sizeof (char *) * q->type->numfields;
  r = (QUEUEROW *) malloc (sz);
  r->queue = q;
  r->rowid = 0;
  for (i = 0; i < q->type->numfields; i++)
    r->field[i] = NULL;
  return (r);
}

/*
 * Free a row
 */
QUEUEROW *queue_row_free (QUEUEROW *r)
{
  int i;

  if (r == NULL)
    return (NULL);
  for (i = 0; i < r->queue->type->numfields; i++)
  {
    if (r->field[i] != NULL)
      free (r->field[i]);
  }
  free (r);
  return (NULL);
}

/*
 * Find the field index with this name
 */
int queue_field_find (QUEUE *q, char *name)
{
  int i;

  if ((q == NULL) || (name == NULL))
    return (-1);
  for (i = 0; i < q->type->numfields; i++)
  {
    if (stricmp (name, q->type->field[i]) == 0)
      return (i);
  }
  return (-1);
}

/*
 * Get a field value by name
 */
char *queue_field_get (QUEUEROW *r, char *name)
{
  int i;

  if (r == NULL)
    return (NULL);
  if ((i = queue_field_find (r->queue, name)) < 0)
    return (NULL);
  return (r->field[i] == NULL ? "" : r->field[i]);
}

/*
 * Set a field by name
 */
int queue_field_set (QUEUEROW *r, char *name, char *value)
{
  int i;

  if (r == NULL)
    return (-1);
  if ((i = queue_field_find (r->queue, name)) < 0)
    return (-1);
  if (i == 0)
    r->rowid = atoi (value);
  r->field[i] = stralloc (r->field[i], value);
  return (i);
}

#ifdef UNITTEST
#undef UNITTEST

int main (int argc, char **argv)
{
  XML *xml;

  if ((xml = xml_parse (PhineasConfig)) == NULL)
    return (-1);
  debug ("attempting initialization\n");
  if (queue_init (xml))
    fatal ("Couldn't initialize from config/Queues.xml!\n");
  debug ("attempting shutdown\n");
  if (queue_shutdown ())
    fatal ("Failed shutdown\n");
  info ("Queue unit test completed\n");
  exit (Errors);
}

#endif
