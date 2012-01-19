/*
 * log.c
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
#include <time.h>
#include <stdarg.h>
#include <sys/stat.h>
#include "log.h"

#define __LOG_C__

LOGGER *dflt_logger = NULL;

LOGGER *log_open (char *name)
{
  LOGGER *logger;
  int sz = 0;
  FILE *fp;
  struct stat st;
  char newpath[MAX_PATH];

  if ((name == NULL) || (*name == 0))
  {
#ifdef CMDLINE
    name = "stdout";
    sz = dup (fileno (stdout));
    fp = fdopen (sz, "w");
#else
    return (NULL);
#endif
  }
  else
  {
    if (stat (name, &st) == 0)
    {
      if ((time (NULL) - st.st_ctime) > 24 * 60 * 60)
      {
	strcpy (newpath, name);
	sz = strlen (name);
	strftime (newpath + sz, MAX_PATH - sz, "%Y%m%d", 
	    localtime (&st.st_ctime));
	rename (name, newpath);
      }
    }
    if ((fp = fopen (name, "a")) == NULL)
      return (NULL);
  }
  sz = strlen (name);
  logger = (LOGGER *) malloc (sizeof (LOGGER) + sz);
  init_mutex (logger);
  logger->level = LOG_DEFAULT;
  strcpy (logger->name, name);
  logger->fp = fp;
  return logger;
}

LOGGER *log_close (LOGGER *logger)
{
  if (logger == NULL)
    return (NULL);
  wait_mutex (logger);
  if (logger->fp != stdout)
    fclose (logger->fp);
  end_mutex (logger);
  destroy_mutex (logger);
  free (logger);
  return (NULL);
}

int log_setlevel (LOGGER *logger, int level)
{
  int l = logger->level;
  logger->level = level;
  return (l);
}

int log_level (LOGGER *logger, char *level)
{
  int l;

  if (stricmp (level, "DEBUG") == 0)
    l = LOG_DEBUG;
  else if (stricmp (level, "INFO") == 0)
    l = LOG_INFO;
  else if (stricmp (level, "WARN") == 0)
    l = LOG_WARN;
  else if (stricmp (level, "ERROR") == 0)
    l = LOG_ERROR;
  else if (stricmp (level, "FATAL") == 0)
    l = LOG_FATAL;
  else
    l = LOG_DEFAULT;
  return (log_setlevel (logger, l));
}

void log (LOGGER *logger, int level, char *file, int line, char *fmt, ...)
{
  int e;
  time_t t;
  va_list ap;
  char *prefix[] = 
  {
    "FATAL", "ERROR", "WARN", "INFO", "DEBUG"
  };
    
  /*
   * only report to current set level
   */
  if ((logger == NULL) || (logger->level < level))
    return 0;

  /*
   * critical unless FATAL!
   */
  if (level)
    wait_mutex (logger);

  /*
   * begin log line and add prefix
   */
  time (&t);
  if (file != NULL)
  {
    fprintf (logger->fp, "%.24s %s[%d] %s-%d: ", 
      ctime(&t), prefix[level],GetCurrentThreadId(), file, line);
  }
  else
  {
    fprintf (logger->fp, "%.24s %s[%d] ", 
      ctime(&t), prefix[level], GetCurrentThreadId());
  }
  /*
   * add this fmt
   */
  va_start (ap, fmt);
  vfprintf (logger->fp, fmt, ap);
  va_end (ap);
  fflush (logger->fp);
  if (level)
    end_mutex (logger);
  else
    exit (1);
}
