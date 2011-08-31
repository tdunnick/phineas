/*
 * task.c
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
#include "task.h"

#ifndef debug
#define debug(fmt...) 
#endif

/*
  printf("DEBUG[%d] %s %d-",GetCurrentThreadId(),__FILE__,__LINE__),\
  printf(fmt)
*/

/*
 * allocate a queue 
 * our event is auto-reset and initially reset
 */
TASKQ *task_allocq (int numthreads, int timeout)
{
  TASKQ *q;

  q = (TASKQ *) malloc (sizeof (TASKQ));
  init_mutex (q);
  init_ready (q, FALSE);
  q->timeout = timeout;
  q->queue = NULL;
  q->pool = NULL;
  q->maxthreads = numthreads;
  q->running = 0;
  q->waiting = 0;
  q->stop = 0;
  return (q);
}

/*
 * free up a queue
 */

TASKQ *task_freeq (TASKQ *q)
{
  TASK *t;

  task_stop (q);
  wait_mutex (q);
  debug ("freeing queue\n");
  while ((t = q->queue) != NULL)
  {
    q->queue = t->next;
    free (t);
  }
  debug ("freeing pool\n");
  while ((t = q->pool) != NULL)
  {
    q->pool = t->next;
    free (t);
  }
  debug ("closing event and critical section\n");
  end_mutex (q);
  destroy_mutex (q);
  destroy_ready (q);
  free (q);
  return (NULL);
}

/*
 * Stop and wait for all threads to exit (assume we are one of them).
 * If stop is already set, return non-zero.
 */
int task_stop (TASKQ *q)
{
  wait_mutex (q);
  if (q->stop)
  {
    end_mutex (q);
    return (-1);
  }
  q->stop = 1;
  debug ("waiting on tasks to exit...\n");
  while (q->waiting || (q->running > 1))
  {
    set_ready (q);
    end_mutex (q);
    sleep (100);
    wait_mutex (q);
  }
  end_mutex (q);
  debug ("stop completed\n");
  return (0);
}

/*
 * reset active tasks
 */
int task_reset (TASKQ *q)
{
  TASK *t;

  wait_mutex (q);
  while ((t = q->queue) != NULL)
  {
    q->queue = t->next;
    t->next = q->pool;
    q->pool = t;
  }
  end_mutex (q);
  return (0);
}

/*
 * start as many threads as might be needed for the current queue
 */
int task_start (TASKQ *q)
{
  int num_tasks = 0;
  TASK *t;

  wait_mutex (q);
  q->stop = 0;
  for (t = q->queue; t != NULL; t = t->next)
    num_tasks++;
  num_tasks -= q->waiting;
  set_ready (q);
  while ((num_tasks-- > 0) && (q->running < q->maxthreads))
  {
    debug ("starting new task\n");
    q->running++;
    t_start (task_run, q);
  }
  end_mutex (q);
  return (0);
}

/*
 * add a task to run, and start a thread for it if needed and not stopped
 * return non-zero if something bad happens
 */
int task_add (TASKQ *q, int (*fn)(void *), void *parm)
{
  TASK *new, **tp;

  wait_mutex (q);
  debug ("getting new task\n");
  if ((new = q->pool) != NULL)
    q->pool = new->next;
  else
    new = (TASK *) malloc (sizeof (TASK));
  new->next = NULL;
  new->fn = fn;
  new->parm = parm;
  debug ("adding task to queue list\n");
  for (tp = &q->queue; *tp != NULL; tp = &(*tp)->next);
  *tp = new;
  end_mutex (q);
  if (q->stop == 0)
    task_start (q);
  return (0);
}

/*
 * This is our actual thread.  It gets tasks off the queue and 
 * runs them until it either times out waiting for a new task,
 * or is told to exit.
 */
void task_run (TASKQ *q)
{
  int v;
  TASK *t;
  int (*fn)(void *p);
  void *parm;

  debug ("new thread started\n");
  wait_mutex (q);
  while (q->stop == 0)		/* get next task from the queue	*/
  { 
    debug ("checking next task\n");
    if ((t = q->queue) != NULL)	/* found something to do	*/
    {
      if ((q->queue = t->next) != NULL)
        set_ready (q);		/* notify others task is ready	*/
      fn = t->fn;		/* note our task		*/
      parm = t->parm;
      t->next = q->pool;	/* add it back to the pool	*/
      q->pool = t;
      debug ("completing task\n");
      end_mutex (q);
      (*fn) (parm);		/* complete the task		*/
      wait_mutex (q);
    }
    else			/* no tasks, go idle		*/
    {
      q->waiting++;		/* note we are waiting		*/
      end_mutex (q);
      debug ("waiting for ready\n");
      v = wait_ready (q);
      wait_mutex (q);
      q->waiting--;
      if (v != TASK_READY)
      {
        debug ("timed out or lost event\n");
        break;	
      }
    }
  }
  /* exit here, note no longer running, regardless		*/
  q->running--;			/* note we no longer run	*/
  end_mutex (q);
  debug ("exiting\n");
  t_exit ();
}
