/*
 * filter.c
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

#ifdef UNITTEST
#include "unittest.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <windows.h>


#include "log.h"
#include "dbuf.h"
#include "util.h"

#ifndef debug
#define debug(fmt...)
#endif

#ifndef t_start
#define STACKSIZE 0x10000
#define t_start(p,e) _beginthread((p),STACKSIZE,(e))
#define t_exit _endthread
#endif

#define P_NOWAIT 1
#define WAIT_CHILD 0

/*
 * execute a program
 */

#define READER 0			/* pipe indexes			*/
#define WRITER 1

typedef struct 				/* for read/write threads	*/
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
  // debug ("begining read...\n");
  while (read (fd, &c, 1) == 1)
  {
    dbuf_putc (b, c);
  }
  // debug ("closing reader %d\n", fd);
  close (fd);
  debug ("filter read %d bytes\n", dbuf_size (b));
}

/*
 * thread to read the filter
 */
void filter_treader (FPARM *p)
{
  if (p->fd >= 0)
    filter_reader (p->fd, p->b);
  debug ("reader thread exiting\n");
  t_exit ();
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
    // debug ("writing %d\n", l);
    r = write (fd, ch, l);
    if (r < 1)
    {
      debug ("fd=%d l=%d\n", fd, l);
      error ("write failed %s\n", strerror (errno));
      break;
    }
    ch += r;
    len -= r;
  }
  // debug ("closing writer %d\n", fd);
  close (fd);
  debug ("filter writer completed\n");
}

/*
 * clean up a pipe
 */
void filter_close (int *pfd)
{
  struct stat st;
  int fd;

  if (((fd = pfd[READER]) != STDIN_FILENO) && (fstat (fd, &st) == 0))
    close (fd);
  if (((fd = pfd[WRITER]) != STDOUT_FILENO) && (fstat (fd, &st) == 0))
    close (fd);
}

/*
 * Create a pipe for communication with the filter.  Do it so that
 * the filter does not inherit the server's side of the pipe.
 * writer set relative to the process we are starting.
 * If we want to read from the pipe, writer is true and vice versa.
 */
int filter_pipe (int *pfd, int writer)
{
  int fd;

  if (_pipe (pfd, BUFSIZ, O_BINARY|O_NOINHERIT) < 0)
    return (-1);
  /*
   * change the side passed to the filter to be inheritable
   */
  fd = dup (pfd[writer]);
  debug ("pipe %s filter=%d/%d caller=%d\n", writer ? "writer" : "reader",
      fd, pfd[writer], pfd[!writer]);
  close (pfd[writer]);
  pfd[writer] = fd;
  return (0);
}

/*
 * Replace command line place holder with file name if given.
 * If no file name, replace with psuedo named pipe.
 * If no place holder open file if given.
 * Return non-zero if fails
 */
int filter_cmd (int *pfd, char *cmd, char *fname, int writer)
{
  char *b, buf[MAX_PATH], fbuf[MAX_PATH];

  pfd[READER] = STDIN_FILENO;
  pfd[WRITER] = STDOUT_FILENO;
    				/* no fname replacement...	*/
  if ((b = strstr (cmd, writer ? "$out" : "$in")) == NULL)
  {
    if (*fname == 0)
      return (filter_pipe (pfd, writer));
    if (writer)
      pfd[WRITER] = creat (fname, S_IWRITE);
    else
      pfd[READER] = open (fname, _O_RDONLY|_O_BINARY);
    return (pfd[writer] < 0);
  }
  				/* fname replaced, need one?	*/
  if (*fname == 0)
    pathf (fname, "tmp/%c%d", writer ? 'W' : 'R', GetCurrentThreadId());
  sprintf (buf, "%.*s\"%s\"%s", b - cmd, cmd, fname, b + (writer ? 4 : 3));
  strcpy (cmd, buf);
  return (pfd[writer] < 0);
}

/*
 * start filter
 *
 * Return a fd to read the process stderr, or < 0 if we can't start.
 * This should probably be wrapped in a critical section?
 */
int filter_start (PROCESS_INFORMATION *pi, char *cmd, int ifd, int ofd)
{
  STARTUPINFO si;
  int e, pfd[2];
  DWORD flags = DETACHED_PROCESS;

  debug ("setting up process info...\n");
  memset (pi, 0, sizeof (PROCESS_INFORMATION));
  memset (&si, 0, sizeof (STARTUPINFO));
  si.cb = sizeof (STARTUPINFO);
  si.dwFlags = STARTF_USESTDHANDLES;
  if (filter_pipe (pfd, WRITER))
    return (-1);
  si.hStdInput = (HANDLE) _get_osfhandle (ifd);
  si.hStdOutput =(HANDLE) _get_osfhandle (ofd);
  si.hStdError = (HANDLE) _get_osfhandle (pfd[WRITER]);

  debug ("running %s\n", cmd);
  e = CreateProcess (NULL, cmd, NULL, NULL, TRUE, flags,
      NULL, NULL, &si, pi);
  close (pfd[WRITER]);
  if (ifd != STDIN_FILENO)
    close (ifd);
  if (ofd != STDOUT_FILENO)
    close (ofd);
  if (e == 0)
  {
    close (pfd[READER]);
    return (-1);
  }
  return (pfd[READER]);
}

/*
 * terminate filter and return exit code
 */
int filter_exit (PROCESS_INFORMATION *pi, int timeout)
{
  DWORD ok;

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
  WaitForSingleObject (pi->hProcess, timeout);
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

/*********************** advertised function ***********************/

/*
 * Run a filter.
 * cmd - command string where $in=input filie and $out=output file
 * fin - input file name
 * in - input buffer
 * fout - output file name
 * out - output buffer
 * err - malloc'ed stderr output
 * timeout - milliseconds to wait for filter
 *
 * return filter exit code
 */
int filter_run (char *cmd, char *fin, DBUF *in, char *fout, DBUF *out, 
    char **err, int timeout)
{
  PROCESS_INFORMATION pi;
  int e, ifd[2], ofd[2];
  FPARM fe, fi;
  char finb[MAX_PATH] = "",
       foutb[MAX_PATH] = "",
       cmdb[MAX_PATH];

  			/* set up command and file names	*/
  cmd = strcpy (cmdb, cmd);
  if (fin == NULL)
    fin = finb;
  if (fout == NULL)
    fout = foutb;
  			/* modify for input and set fd		*/
  if (filter_cmd (ifd, cmd, fin, READER) < 0)
  {
    error ("Failed to create filter input pipe for %s\n", cmd);
    return (-1);
  }
  			/* create the input file if needed	*/
  if (*finb)
    writefile (finb, dbuf_getbuf (in), dbuf_size (in));
  			/* modify for output and set fd		*/
  if (filter_cmd (ofd, cmd, fout, WRITER) < 0)
  {
    filter_close (ifd);
    if (*finb)
      unlink (finb);
    error ("Failed to create filter output pipe for %s\n", cmd);
    return (-1);
  }
  			/* read to run				*/
  if ((fe.fd = filter_start (&pi, cmd, ifd[READER], ofd[WRITER])) < 0)
  {
    filter_close (ifd);
    filter_close (ofd);
    if (*finb)
      unlink (finb);
    debug ("failed starting %s\n", cmd);
    return (-1);
  }
  			/* catch the stderr			*/
  fe.b = dbuf_alloc ();
  t_start (filter_treader, &fe);
  			/* catch stdout if piped		*/
  if (*fout == 0)
  {
    fi.fd = ofd[READER];
    fi.b = out;
    t_start (filter_treader, &fi);
  }
  			/* write stdin if piped			*/
  if (*fin == 0)
    filter_writer (ifd[WRITER], in);
  			/* wait for exit			*/
  e = filter_exit (&pi, timeout);
  			/* manage any error condition		*/
  debug ("error:%s\n", dbuf_getbuf (fe.b));
  if (err != NULL)
    *err = dbuf_extract (fe.b);
  else
    dbuf_free (fe.b);
  			/* clean up				*/
  filter_close (ifd);
  filter_close (ofd);
  if (*finb)
    unlink (finb);
  			/* read psuedo named pipe to buffer	*/
  if (*foutb)
  {
    int len;
    char *ch;
    if ((ch = readfile (foutb, &len)) != NULL)
      dbuf_setbuf (out, ch, len);
    unlink (foutb);
  }
  return (e);
}

#ifdef UNITTEST
#undef UNITTEST
#undef debug

#include "dbuf.c"
#include "util.c"

int dbuf_cmp (DBUF *a, DBUF *b)
{
  char *ap, *bp;
  int i, sz;

  ap = dbuf_getbuf (a);
  bp = dbuf_getbuf (b);
  sz = dbuf_size (a);
  i = dbuf_size (b);
  if (sz != i)
    error ("size mismatch %d/%d\n", sz, i);
  if (sz < i)
    sz = i;
  for (i = 0; i < sz; i++)
  {
    if (*ap != *bp)
    {
      error ("difference at %d - %c/%c\n", i, *ap, *bp);
      break;
    }
    ap++;
    bp++;
  }
  return (Errors);
}

int chk_open ()
{
  int i, n;
  struct stat st;

  debug ("checking open files\n");
  for (i = 0; i < 50; i++)
  {
    if (fstat (i, &st) == 0)
    {
      if (i > 2)
	error ("fd %d still open\n", i);
    }
  }
  return (Errors);
}

/*
#undef fatal
#define fatal(fmt...) printf("FATAL %s %d-",__FILE__,__LINE__),\
  printf (fmt), chk_open ()
*/

#define TESTFILE "../console/help.html"

int main (int argc, char **argv)
{
  int e;
  DBUF *b, *d;
#define MSWAIT 2000
  b = dbuf_alloc ();
  d = dbuf_alloc ();

  loadpath ("../");
  dbuf_printf (d, "The quick brown fox");
  if (e = filter_run ("cat", NULL, d, NULL, b, NULL, MSWAIT)
      || chk_open ())
    fatal ("buf/buf %d\n", e);
  debug ("buf/buf read:'%.*s'\n", dbuf_size (b), dbuf_getbuf(b));
  dbuf_clear (b);
  dbuf_clear (d);
  if (filter_run ("cat.exe", TESTFILE, NULL, NULL, b, NULL, MSWAIT) 
      || chk_open())
    fatal (TESTFILE "/buf\n");
  debug (TESTFILE "/buf read:%d chars\n", dbuf_size (b));
  // dbuf_printf (b, "--------------------\r\n");
  if (filter_run ("cat.exe " TESTFILE, NULL, NULL, NULL, b, NULL, MSWAIT)
      || chk_open ())
    fatal ("cat.exe" TESTFILE "/buf\n", "cat");
  debug ("cat.exe" TESTFILE "/buffer read:%d chars\n", dbuf_size (b));
  if (filter_run ("cat.exe $in", TESTFILE, NULL, NULL, b, NULL, MSWAIT)
      || chk_open ())
    fatal ("cat.exe $in/buf\n", "cat");
  debug ("cat.exe $in/buf read:%d chars\n", dbuf_size (b));
  if (filter_run ("cat", NULL, b, "foo.txt", NULL, NULL, MSWAIT)
      || chk_open ())
    fatal ("buf/foo.txt\n");
  dbuf_clear (d);
  if (filter_run ("cat foo.txt", NULL, NULL, NULL, d, NULL, MSWAIT)
    || chk_open () || dbuf_cmp (b, d))
    fatal ("cat foo.txt/buf\n");
  unlink ("foo.txt");
  if (filter_run ("cat $in", NULL, b, "foo.txt", NULL, NULL, MSWAIT)
      || chk_open ())
    fatal ("cat $in (buf)/foo.txt\n");
  dbuf_clear (d);
  if (filter_run ("cat foo.txt", NULL, NULL, NULL, d, NULL, MSWAIT)
      || chk_open () || dbuf_cmp (b, d))
    fatal ("cat foo.txt/buf\n");
  unlink ("foo.txt");
  if (filter_run ("cat -o bar.txt", NULL, b, NULL, NULL, NULL, MSWAIT)
      || chk_open ())
    fatal ("buf/cat -o bar.txt\n");
  dbuf_clear (d);
  if (filter_run ("cat", "bar.txt", NULL, NULL, d, NULL, MSWAIT)
      || chk_open () || dbuf_cmp (b, d))
    fatal ("bar.txt/buf\n");
  unlink ("bar.txt");
  if (filter_run ("cat -o $out", NULL, b, "bar.txt", NULL, NULL, MSWAIT)
      || chk_open ())
    fatal ("cat -o $out/bar.txt\n");
  dbuf_clear (d);
  if (filter_run ("cat", "bar.txt", NULL, NULL, d, NULL, MSWAIT)
      || chk_open () || dbuf_cmp (b, d))
    fatal ("bar.txt/buf\n");
  if (filter_run ("cat", "bar.txt", NULL, "foo.txt", NULL, NULL, MSWAIT))
    fatal ("bar.txt/foo.txt\n");
  dbuf_clear (d);
  if (filter_run ("cat foo.txt", NULL, NULL, NULL, d, NULL, MSWAIT)
      || dbuf_cmp (d, b))
    fatal ("cat foo.txt/buf\n");
  dbuf_clear (d);
  if (filter_run ("cat -o $out $in", "foo.txt", NULL, NULL, d, NULL, MSWAIT)
    || chk_open () || dbuf_cmp (b, d))
    fatal ("cat -o $out(buf) $in/buf\n");
  unlink ("bar.txt");
  unlink ("foo.txt");
  dbuf_free (b);
  dbuf_free (d);
  info ("%s %s\n", argv[0], Errors?"failed":"passed");
  exit (Errors);
}

#endif
