/*
 * qpoller.c
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
#define __SENDER__
#endif

#ifdef __SENDER__

#include <stdio.h>

#ifdef UNITTEST
#undef UNITTEST
#include "unittest.h"
#include "util.c"
#include "dbuf.c"
#include "xml.c"
#include "task.c"
#include "queue.c"
#include "fileq.c"
#define UNITTEST
#define debug _DEBUG_
#else
#include "util.h"
#include "log.h"
#include "find.h"
#include "task.h"
#endif
#include "qpoller.h"

#ifndef debug
#define debug(fmt...)
#endif

#define QP_INFO "Phineas.QueueInfo"
#define QP_QUEUE QP_INFO".Queue"

typedef struct qpoller
{
  struct qpoller *next;
  int (*proc) (XML *, QUEUEROW *);
  char type[1];
} QPOLLER;

QPOLLER *Qpoller = NULL;

typedef struct qpollerjob
{
  struct qpollerjob *next;
  int (*proc) (XML *, QUEUEROW *);
  XML *xml;
  QUEUEROW *row;
} QPOLLERJOB;

QPOLLERJOB *QpollerJobs = NULL;

/*
 * run a poller
 */
int qpoller_run (void *p)
{
  QPOLLERJOB *job;

  job = (QPOLLERJOB *) p;
  job->proc (job->xml, job->row);
  job->proc = NULL;
  return (0);
}

/*
 * start up a poller
 */
int qpoller_start (QPOLLER *poller, XML *xml, QUEUEROW *row, TASKQ *q)
{
  QPOLLERJOB **p;

  for (p = &QpollerJobs; *p != NULL; p = &(*p)->next)
  {
    if ((*p)->proc == NULL)
      break;
  }
  if (*p == NULL)
  {
    *p = (QPOLLERJOB *) malloc (sizeof (QPOLLERJOB));
    (*p)->next = NULL;
  }
  (*p)->proc = poller->proc;
  (*p)->xml = xml;
  (*p)->row = row;
  task_add (q, qpoller_run, (void *) *p);
  debug ("starting processor %s for %s row %d\n", poller->type,
    row->queue->name, row->rowid);
  return (0);
}

/*
 * register a processor for a queue type
 */
qpoller_register (char *type, int (*proc) (XML *, QUEUEROW *))
{
  QPOLLER *n, **p;

  n = (QPOLLER *) malloc (sizeof (QPOLLER) + strlen (type));
  n->next = NULL;
  n->proc = proc;
  strcpy (n->type, type);
  for (p = &Qpoller; *p != NULL; p = &(*p)->next);
  *p = n;
}

/*
 * Poll and process one queue
 */
int qpoller_poll (XML *xml, int mapid, TASKQ *tq)
{
  int mpos;
  char *ch;
  QPOLLER *p;
  QUEUE *q;
  QUEUEROW *r;
  char mpath[80];

  mpos = sprintf (mpath, QP_QUEUE"[%d].", mapid);
  strcpy (mpath + mpos, "Type");
  ch = xml_get_text (xml, mpath);
  for (p = Qpoller; p != NULL; p = p->next)
  {
    if (strcmp (p->type, ch) == 0)
      break;
  }
  if (p == NULL)
  {
    debug ("No processor found for Queue type %s\n", ch);
    return (-1);
  }
  strcpy (mpath + mpos, "Name");
  ch = xml_get_text (xml, mpath);
  debug ("Processor %s for Queue %s\n", p->type, ch);
  if ((q = queue_find (ch)) == NULL)
  {
    debug ("Can't find queue %s\n", ch);
    return (-1);
  }
  while ((r = queue_pop (q)) != NULL)
  {
    qpoller_start (p, xml, r, tq);
  }
  return (0);
}

/*
 * Poll all queues...
 * a thread, expected to be started from the TASKQ.  Note you must
 * re-register processors once this task exits.
 *
 * Note we expect sender_xml to have QueueInfo embedded!
 */
int qpoller_task (void *parm)
{
  int i,
      poll_interval,
      num_queues;
  QPOLLER *p;
  QPOLLERJOB *j;
  TASKQ *q;
  XML *xml = (XML *) parm;

  info ("Queue Poller starting\n");
  num_queues = xml_count (xml, QP_QUEUE);
  if ((poll_interval = xml_get_int (xml, QP_INFO".PollInterval")) < 1)
    poll_interval = 5;
  poll_interval *= 1000;
  if ((i = xml_get_int (xml, QP_INFO".MaxThreads")) < 1)
    i = 1;
  q = task_allocq (i, poll_interval);
  debug ("%d queues %d interval\n", num_queues, poll_interval);
  while (!phineas_status ())
  {
    for (i = 0; i < num_queues; i++)
    {
      qpoller_poll (xml, i, q);
    }
    sleep (poll_interval);
  }
  debug ("Queue Poller shutting down...\n");
  task_stop (q);
  task_freeq (q);
  while ((j = QpollerJobs) != NULL)
  {
    QpollerJobs = j->next;
    free (j);
  }
  while ((p = Qpoller) != NULL)
  {
    Qpoller = p->next;
    free (p);
  }
  info ("Queue Poller exiting\n");
  return (0);
}

#ifdef UNITTEST

int ran = 0;
int phineas_status  ()
{
  return (ran++ > 2);
}

int test_qprocessor (XML *x, QUEUEROW *r)
{
  debug ("processing row %d for %s\n", r->rowid, r->queue->name);
  return (0);
}

int main (int argc, char **argv)
{
  XML *xml;
  char *ch;

  xml = xml_parse (PhineasConfig);
  queue_register ("FileQueue", fileq_connect);
  queue_init (xml);
  debug ("begin registration...\n");
  qpoller_register ("EbXmlSndQ", test_qprocessor);
  qpoller_task (xml);
  xml_free (xml);
  info ("%s unit test completed\n", argv[0]);
}

#endif /* UNITTEST */
#endif /* __SENDER__ */
