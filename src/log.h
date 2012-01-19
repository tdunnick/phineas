/*
 * log.h - general logging
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
 */

#ifndef __LOG__
#define __LOG__

#include <stdio.h>
#include "task.h"

/*
 * a logger
 */
typedef struct logger
{
  MUTEX mutex;
  FILE *fp;
  int level;
  char name[1];
} LOGGER;

#define LOG_FATAL 0
#define LOG_ERROR 1
#define LOG_WARN 2
#define LOG_INFO 3
#define LOG_DEBUG 4
#ifdef DEBUG
#define LOG_DEFAULT LOG_DEBUG
#else
#define LOG_DEFAULT LOG_INFO
#endif

LOGGER *log_open (char *name);
LOGGER *log_close (LOGGER *logger);
int log_setlevel (LOGGER *logger, int level);
int log_level (LOGGER *logger, char *level);
void log (LOGGER *logger, int level, char *file, int line, char *fmt, ...);

#ifndef __LOG_C__
extern LOGGER *dflt_logger;
#ifndef LOGFILE
#define LOGFILE dflt_logger
#endif
#define fatal(fmt...) log(LOGFILE, LOG_FATAL, __FILE__, __LINE__, fmt)
#define error(fmt...) log(LOGFILE, LOG_ERROR, __FILE__, __LINE__, fmt)
#define warn(fmt...) log(LOGFILE, LOG_WARN, __FILE__, __LINE__, fmt)
#define info(fmt...) log(LOGFILE, LOG_INFO, __FILE__, __LINE__, fmt)
#ifdef DEBUG
#define debug(fmt...) \
  log(LOGFILE, LOG_DEBUG, __FILE__, __LINE__, fmt)
#else
#define debug(fmt...)
#endif /* DEBUG */
#endif /* __LOG_C__ */
#endif /* __LOG__ */
