/*
 * fpoller.h
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
#ifndef __FPOLLER__
#define __FPOLLER__

#include "xml.h"

/*
 * register a folder processor with a folder name
 */
int fpoller_register (char *name, int (*proc)(XML *, char *, char *));

/*
 * Poll all folder maps...
 * a thread, expected to be started from the TASKQ.  Note you must
 * re-register processors after this task exits.
 */
int fpoller_task (void *parm);

#endif /* __FPOLLER__ */
