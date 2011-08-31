/*
 * filter.c
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
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <windows.h>


#ifdef UNITTEST
#undef UNITTEST
#include "unittest.h"
#include "dbuf.c"
#define UNITTEST
#define debug _DEBUG_
#else
#include "log.h"
#include "dbuf.h"
#endif

#ifndef debug
#define debug(fmt...)
#endif

#define P_NOWAIT 1
#define WAIT_CHILD 0

/*
 * execute a cgi script
 */

#define READER 0			/* pipe indexes			*/
#define WRITER 1

typedef struct 
{
  int fd;
  DBUF *b;
} FPARM;


/* catch the filter output */
void filter_reader (int fd, DBUF *b)
{
  unsigned char c;
  int state = 0;

  if (b == NULL)
    return;
  debug ("begining cgi read...\n");
  while (read (fd, &c, 1) == 1)
  {
    dbuf_putc (b, c);
  }
  debug ("closing reader %d\n", fd);
  close (fd);
  debug ("filter read %d bytes\n", dbuf_size (b));
}


/* send the filter input */
void filter_writer (int fd, DBUF *b)
{
  unsigned char *ch;
  int l, r, len;
  
  if (b == NULL)
    return;
  ch = dbuf_getbuf (b);
  len = dbuf_size (b);
  while (len > 0)
  {
    l = len > BUFSIZ ? BUFSIZ : len;
    debug ("writing %d\n", l);
    r = write (fd, ch, l);
    if (r < 1)
    {
      error ("write failed %s\n", strerror (errno));
      break;
    }
    ch += r;
    len -= r;
  }
  debug ("closing writer %d\n", fd);
  close (fd);
  debug ("filter writer completed\n");
}

void filter_twriter (FPARM *p)
{
  filter_writer (p->fd, p->b);
  endthread ();
}

/*
 * Create a pipe for communication with the filter.  Do it so that
 * the filter does not inherit the server's side of the pipe.
 */
int filter_pipe (int *pfd, int dflag)
{
  int fd;

  if (_pipe (pfd, BUFSIZ, O_BINARY|O_NOINHERIT) < 0)
    return (-1);
  /*
   * change the side passed to the filter to be inheritable
   */
  fd = dup (pfd[dflag]);
  debug ("new fd=%d old fd=%d (%d)\n", fd, pfd[dflag], pfd[!dflag]);
  close (pfd[dflag]);
  pfd[dflag] = fd;
  return (0);
}

/*
 * start filter
 *
 * Note we dup the two file descriptors, since the underlying
 * pipes should be hidden (O_NOINHERIT) from the new process.
 * This should probably be wrapped in a critical section?
 */
int filter_start (PROCESS_INFORMATION *pi, char *cmd, int ifd, int ofd)
{
  STARTUPINFO si;
  int e;
  DWORD flags = DETACHED_PROCESS;

  debug ("setting up process info...\n");
  memset (pi, 0, sizeof (PROCESS_INFORMATION));
  memset (&si, 0, sizeof (STARTUPINFO));
  si.cb = sizeof (STARTUPINFO);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = (HANDLE) _get_osfhandle (ifd);
  si.hStdOutput =(HANDLE) _get_osfhandle (ofd);
  si.hStdError = si.hStdOutput;

  debug ("running %s\n", cmd);
  e = CreateProcess (NULL, cmd, NULL, NULL, TRUE, flags,
      NULL, NULL, &si, pi);
  return (e == 0);
}

/*********************** advertised functions ***********************/

/*
 * Read file fname using filter cmd to buffer in
 */
int filter_read (char *cmd, char *fname, DBUF *in)
{
  PROCESS_INFORMATION pi;
  int ifd, e, pfd[2];

  if (fname == NULL)
    ifd = fileno (stdin);
  else if ((ifd = open (fname, _O_RDONLY|_O_BINARY)) < 0)
  {
    error ("can't open %s\n", fname);
    return (-1);
  }
  /* get pipe for reading filter's stdout */
  debug ("creating pipes...\n");
  if (filter_pipe (pfd, WRITER))
  {
    error ("can't create input pipe for %s\n", cmd);
    close (ifd);
    return (-1);
  }
  e = filter_start (&pi, cmd, ifd, pfd[WRITER]);
  if (fname != NULL)
    close (ifd);
  close (pfd[WRITER]);
  if (e)
  {
    debug ("failed starting %s\n", cmd);
    close (pfd[READER]);
    return (-1);
  }
  filter_reader (pfd[READER], in);
  return (filter_exit (&pi));
}

/*
 * write file fname using filter cmd from buffer out
 */
int filter_write (char *cmd, char *fname, DBUF *out)
{
  PROCESS_INFORMATION pi;
  int flags,
      ofd, e, pfd[2];

  if (fname == NULL)
    ofd = fileno (stdout);
  else if ((ofd = creat (fname, S_IWRITE)) < 0)
  {
    error ("can't open %s\n", fname);
    return (-1);
  }
  /* get pipe for reading filter's stdout */
  debug ("creating pipes...\n");
  if (filter_pipe (pfd, READER))
  {
    error ("can't create input pipe for %s\n", cmd);
    close (ofd);
    return (-1);
  }
  e = filter_start (&pi, cmd, pfd[READER], ofd);
  if (fname != NULL)
    close (ofd);
  close (pfd[READER]);
  if (e)
  {
    close (pfd[WRITER]);
    return (-1);
  }
  filter_writer (pfd[WRITER], out);
  return (filter_exit (&pi));
}


/*
 * filter read/write fully buffered
 */
int filter_buf (char *cmd, DBUF *in, DBUF *out)
{
  PROCESS_INFORMATION pi;
  int flags,
      ofp[2], e, ifp[2];

  /* get pipe for reading filter's stdout */
  debug ("creating pipes...\n");
  if (filter_pipe (ifp, READER))
  {
    error ("can't create input pipe for %s\n", cmd);
    return (-1);
  }
  if (filter_pipe (ofp, WRITER))
  {
    error ("Can't crate output pipe\n");
    return (-1);
  }
  e = filter_start (&pi, cmd, ifp[READER], ofp[WRITER]);
  close (ifp[READER]);
  close (ofp[WRITER]);
  if (e)
  {
    close (ifp[WRITER]);
    return (-1);
  }
  filter_writer (ifp[WRITER], out);
  filter_reader (ofp[READER], in);
  return (filter_exit (&pi));
}

/*
 * filter using file system
 */
int filter_file (char *cmd, char *iname, char *oname)
{
  PROCESS_INFORMATION pi;
  int flags,
      ifd, ofd, e;

  if (iname == NULL)
    ifd = fileno (stdin);
  else if ((ifd = open (iname, _O_RDONLY|_O_BINARY)) < 0)
  {
    error ("can't open %s\n", iname);
    return (-1);
  }

  if (oname == NULL)
    ofd = fileno (stdout);
  else if ((ofd = creat (oname, S_IWRITE)) < 0)
  {
    error ("can't open %s\n", oname);
    return (-1);
  }
  e = filter_start (&pi, cmd, ifd, ofd);
  if (iname != NULL)
    close (ifd);
  if (oname != NULL)
    close (ofd);
  return (filter_exit (&pi));
}


/*
 * terminate filter and return exit code
 */
int filter_exit (PROCESS_INFORMATION *pi)
{
  DWORD ok;
  int foobar;

  debug ("exiting filter...\n");
  if (pi == NULL)
  {
    error ("NULL PROCESS_INFORMATION\n");
    return (-1);
  }
  if (!pi->hProcess)
  {
    error ("No connected process\n");
    return (-1);
  }
  WaitForSingleObject (pi->hProcess, 1000);
  if (!GetExitCodeProcess (pi->hProcess, &ok))
  {
    ok = STILL_ACTIVE;
    error ("Can't get process exit code - %d\n", GetLastError ());
  }
  if (ok == STILL_ACTIVE)
  {
    error ("Filter failed to terminate\n");
    TerminateProcess (pi->hProcess, ok = 1);
  }
  else
  {
    debug ("exit code was %d\n", ok);
    ok = 0;
  }
  CloseHandle (pi->hThread);
  CloseHandle (pi->hProcess);
  return (ok);
}

#ifdef UNITTEST

int dbuf_cmp (DBUF *a, DBUF *b)
{
  char *ap, *bp;
  int e, i, sz;

  ap = dbuf_getbuf (a);
  bp = dbuf_getbuf (b);
  sz = dbuf_size (a);
  i = dbuf_size (b);
  if (sz != i)
  {
    e++;
    error ("size mismatch %d/%d\n", sz, i);
  }
  if (sz < i)
    sz = i;
  for (i = 0; i < sz; i++)
  {
    if (*ap != *bp)
    {
      e++;
      error ("difference at %d - %c/%c\n", i, *ap, *bp);
      break;
    }
    ap++;
    bp++;
  }
  return (-e);
}

chk_open ()
{
  int i, n;
  struct stat st;

  debug ("checking open files\n");
  for (i = 0; i < 50; i++)
  {
    if (fstat (i, &st) == 0)
    {
      if (i > 2)
	info ("fd %d still open\n", i);
    }
  }
}

/*
#undef fatal
#define fatal(fmt...) printf("FATAL %s %d-",__FILE__,__LINE__),\
  printf (fmt), chk_open ()
*/
int main (int argc, char **argv)
{
  DBUF *b, *d;

  b = dbuf_alloc ();
  d = dbuf_alloc ();

  dbuf_printf (d, "The quick brown fox");
  if (filter_buf ("cat", b, d))
    fatal ("Failed buffered filter\n");
  debug ("read:\n'%.*s'\n", dbuf_size (b), dbuf_getbuf(b));
  dbuf_clear (b);
  dbuf_clear (d);
  if (filter_read ("cat.exe", "cc.bat", b))
    fatal ("failed read\n");
  debug ("read:\n'%.*s'\n", dbuf_size (b), dbuf_getbuf(b));
  dbuf_printf (b, "--------------------\r\n");
  if (filter_read ("cat.exe cc.bat", NULL, b))
    fatal ("failed read with NULL file\n", "cat");
  debug ("read:\n'%.*s'\n", dbuf_size (b), dbuf_getbuf(b));
  if (filter_write ("cat", "foo.txt", b))
    fatal ("failed write\n");
  if (filter_read ("cat foo.txt", NULL, d))
    fatal ("failed read with NULL file\n");
  dbuf_cmp (b, d);
  unlink ("foo.txt");
  filter_write ("cat -o bar.txt", NULL, b);
  dbuf_clear (d);
  if (filter_read ("cat", "bar.txt", d))
    fatal ("failed read with NULL file\n");
  dbuf_cmp (b, d);
  if (filter_file ("cat", "bar.txt", "foo.txt"))
    fatal ("failed file filter\n");
  dbuf_clear (d);
  if (filter_read ("cat foo.txt", NULL, d))
    fatal ("failed read with NULL file\n");
  dbuf_cmp (b, d);
  unlink ("bar.txt");
  unlink ("foo.txt");
  dbuf_free (b);
  dbuf_free (d);
  chk_open ();
  info ("%s unit test completed\n", argv[0]);
}

#endif
