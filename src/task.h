/*
 * task.h
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
#ifndef __TASK__
#define __TASK__
#include <windows.h>

/*
 * threadin tasks
 */

#define STACKSIZE 0x10000	/* smallest thread stack supported	*/

/*
 * some defines to abstract window IPC
 */
#define MUTEX CRITICAL_SECTION
#define init_mutex(o) InitializeCriticalSection (&o->mutex);
#define destroy_mutex(o) DeleteCriticalSection (&o->mutex);
#define wait_mutex(o) EnterCriticalSection (&o->mutex)
#define end_mutex(o) LeaveCriticalSection (&o->mutex)
#define READY HANDLE
#define TASK_READY WAIT_OBJECT_0
#define init_ready(o,m) o->ready = CreateEvent (NULL,m,FALSE,NULL)
#define destroy_ready(o) CloseHandle (o->ready)
#define wait_ready(o) WaitForSingleObject (o->ready,o->timeout)
#define set_ready(o) SetEvent (o->ready)
#define reset_ready(o) ResetEvent (o->ready)
#define t_start(fn,p) _beginthread (fn,STACKSIZE,p)
#define t_exit _endthread

/*
 * a task for a thread to perform
 */
typedef struct task
{
  struct task *next;		/* kept in a list			*/
  int (*fn) (void *p);		/* function to call			*/
  void *parm;			/* paramter to pass			*/
} TASK;

/*
 * a queue for tasks - the semaphore indicates the number of tasks that
 * are ready and waiting to run.
 */
typedef struct taskq
{
  MUTEX mutex;
  READY ready;			/* set when a task is ready		*/
  int timeout;			/* for breaking ready wait		*/
  TASK *queue;			/* tasks queued to run			*/
  TASK *pool;			/* a pool of available tasks		*/
  int maxthreads;		/* most threads allowed			*/
  int running;			/* threads running a task		*/
  int waiting;			/* threads waiting for a task		*/
  int stop;			/* exit threads if set			*/
} TASKQ;

/*
 * prototypes
 */
TASKQ *task_allocq (int numthreads, int timeout);
TASKQ *task_freeq (TASKQ *q);
int task_stop (TASKQ *q);
int task_start (TASKQ *q);
int task_reset (TASKQ *q);
int task_add (TASKQ *q, int (*fn)(void *), void *p);
void task_run (TASKQ *q);
#define task_running(q) ((q)->running)
#define task_waiting(q) ((q)->waiting)
#define task_stopping(q) ((q)->stop)

#endif /* __TASK__*/
