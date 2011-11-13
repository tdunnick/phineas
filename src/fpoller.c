/*
 * fpoller.c
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
#include "queue.c"
#include "fileq.c"
#include "find.c"
#define UNITTEST
#define debug _DEBUG_
#else
#include "util.h"
#include "log.h"
#include "find.h"
#include "task.h"
#endif
#include "fpoller.h"

#ifndef debug
#define debug(fmt...)
#endif

#define MAP "Phineas.Sender.MapInfo.Map"

/* the processor list */
typedef struct fpoller
{
  struct fpoller *next;
  int (*proc) (XML *xml, char *prefix, char *fname);
  char name[1];
} FPOLLER;

FPOLLER *Fpoller = NULL;

/*
 * register a folder processor with a folder name
 */
int fpoller_register (char *name, int (*proc)(XML *, char *, char *))
{
  FPOLLER *n, **p;

  n = (FPOLLER *) malloc (sizeof (FPOLLER) + strlen (name));
  n->next = NULL;
  n->proc = proc;
  strcpy (n->name, name);
  for (p = &Fpoller; *p != NULL; p = &(*p)->next);
  *p = n;
}

/*
 * Poll and process one folder
 */
int fpoller_poll (XML *xml, int mapid)
{
  int mpos;
  char *ch;
  FPOLLER *p;
  FINDNAME *f;
  char mpath[80];
  char fpath[MAX_PATH];

  mpos = sprintf (mpath, "%s[%d].", MAP, mapid);
  strcpy (mpath + mpos, "Processor");
  ch = xml_get_text (xml, mpath);
  for (p = Fpoller; p != NULL; p = p->next)
  {
    if (strcmp (ch, p->name) == 0)
      break;
  }
  if (p == NULL)
  {
    debug ("no folder processor found for %s\n", ch);
    return (-1);
  }
  strcpy (mpath + mpos, "Folder");
  pathf (fpath, "%s", xml_get_text (xml, mpath));
  debug ("Folder is %s\n", fpath);
  /*
   * get a directory listing
   */
  f = find (fpath, 0);
  while (find_next (&f, fpath) != NULL)
  {
    mpath[mpos] = 0;
    p->proc (xml, mpath, fpath);
  }
}

/*
 * Poll all folder maps...
 * a thread, expected to be started from the TASKQ.  Note you must
 * re-register processors after this task exits.
 */
int fpoller_task (void *parm)
{
  int i,
      poll_interval,
      num_maps;
  FPOLLER *p;
  XML *xml = (XML *) parm;

  info ("Folder Poller starting\n");
  num_maps = xml_count (xml, MAP);
  if ((poll_interval = xml_get_int (xml, "Phineas.Sender.PollInterval")) < 1)
    poll_interval = 5;
  debug ("%d maps %d interval\n", num_maps, poll_interval);
  poll_interval *= 1000;
  while (!phineas_status ())
  {
    for (i = 0; i < num_maps; i++)
    {
      fpoller_poll (xml, i);
    }
    sleep (poll_interval);
  }
  while ((p = Fpoller) != NULL)
  {
    Fpoller = p->next;
    free (p);
  }
  info ("Folder Poller exiting\n");
  return (0);
}

#ifdef UNITTEST
int ran = 0;
int phineas_status  ()
{
  return (ran++ > 2);
}

int test_processor (XML *xml, char *prefix, char *fname)
{
  debug ("processing %s for %s\n", fname, prefix);
  return (0);
}

int main (int argc, char **argv)
{
  XML *xml;

  xml = xml_parse (PhineasConfig);
  queue_init (xml);
  fpoller_register ("ebxml", test_processor);
  fpoller_task (xml);
  xml_free (xml);
  info ("%s unit test completed\n", argv[0]);
}

#endif /* UNITTEST */
#endif /* __SENDER__ */
