/*
 * qpoller.h
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
#ifndef __QPOLLER__
#define __QPOLLER__

#include "xml.h"
#include "queue.h"

/*
 * register a processor for a queue type
 */
qpoller_register (char *type, int (*proc) (XML *, QUEUEROW *));

/*
 * Poll all queues...
 * a thread, expected to be started from the TASKQ.  Note you must
 * re-register processors once this task exits.
 *
 * Note we expect sender_xml to have QueueInfo embedded!
 */
int qpoller_task (void *parm);

#endif /* __QPOLLER */
