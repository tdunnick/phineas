/*
 * server.c
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
 *
 * Integrated HTTP server for use with stand-alone PHINEAS
 */

#ifdef UNITTEST
#ifndef SERVER
#define SERVER
#endif
#endif

#ifdef SERVER

#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include "util.h"
#include "log.h"
#include "dbuf.h"
#include "task.h"
#include "net.h"
#include "ebxml.h"

/*
 * a TASK parameter
 */
typedef struct
{
  NETCON *conn;
  XML *xml;
} SERVERPARM;

/*
 * configure SSL
 */
SSL_CTX *server_ctx (XML *xml)
{
  SSL_CTX *ctx;
  char *ch,
       *passwd,
       cert[MAX_PATH], 
       key[MAX_PATH], 
       auth[MAX_PATH];

  info ("Initializing SSL context\n");
  ch = xml_get_text (xml, "Phineas.Server.SSL.CertFile");
  if (*ch == 0)
    return (NULL);
  pathf (cert, "%s", ch);
  ch = xml_get_text (xml, "Phineas.Server.SSL.KeyFile");
  if (*ch == 0)
    strcpy (key, cert);
  else
    pathf (key, "%s", ch);
  passwd = xml_get_text (xml, "Phineas.Server.SSL.Password");
  if (*passwd == 0)
    passwd = NULL;
  ch = xml_get_text (xml, "Phineas.Server.SSL.AuthFile");
  if (*ch == 0)
    ch = NULL;
  else
    ch = pathf (auth, "%s", ch);
  if ((ctx = net_ctx (cert, key, passwd, ch, 1)) == NULL)
    error ("Failed getting SSL context for server\n");
  return (ctx);
}

/*
 * Receive an incoming message
 */
DBUF *server_receive (NETCON *conn)
{
  long n, sz;
  char c, *ch;
  DBUF *b;

  b = dbuf_alloc ();
  n = 0;
  /*
   * read to end of message request header - empty line
   */
  while (net_read (conn, &c, 1) == 1)
  {
    dbuf_putc (b, c);
    if (c == '\n')
    {
      if (n++ == 1)
        break;
    }
    else if (c != '\r')
      n = 0;
  }
  if (n != 2)
  {

    if (dbuf_size (b))
    {
      dbuf_printf (b, "%s", n == 1 ? "\r\n" : "\r\n\r\n");
      warn ("can't find end of request header: %.*s", 
	dbuf_size (b), dbuf_getbuf (b));
    }
    else
    {
      warn ("socket timed out - nothing read\n");
    }
    n = 0;
  }
  else
  {
    ch = strnstr (dbuf_getbuf (b), "Content-Length: ", dbuf_size (b));
    if (ch == NULL)
      n = 0;
    else
      n = atol (ch + 16);
  }
  debug ("expecting %d bytes\n", n);
  if (n > 0)
  {
    sz = dbuf_size (b);
    dbuf_setsize (b, sz + n + 1);
    ch = dbuf_getp (b, sz);
    if ((sz = net_read (conn, ch, n)) != n)
      warn ("expected %d bytes but read %d\n", n, sz);
    dbuf_setsize (b, sz + n);
  }
  debug ("returning %d bytes\n", dbuf_size (b));
  return (b);
}

/*
 * format up an HTTP response and return it
 */
DBUF *server_respond (int code, char *fmt, ...)
{
  int l, len;
  va_list ap;
  char *ch;
  DBUF *b;
  char buf[1024];

  b = dbuf_alloc ();
  va_start (ap, fmt);
  len = vsnprintf (buf, 1024, fmt, ap);
  va_end (ap);
  if (len > 0)
  {
    len += 22;
  }
  else
  {
    *buf = 0;
    len = 22;
  }
  dbuf_printf (b, "Status: %d\r\n"
    "Content-Length: %d\r\n"
    "Connection: Close\r\n\r\n<html><body>%s</body></html>",
    code, len, buf);
  debug ("response:\n%s\n", dbuf_getbuf (b));
  return (b);
}

/*
 * return response for a request
 */
DBUF *server_response (XML *xml, char *req)
{
  char *url, *ch;
  DBUF *console_response (XML *, char *);
  extern char Software[];

  if (strstarts (req, "GET "))
    url = req + 4;
  else if (strstarts (req, "POST "))
    url = req + 5;
  else
    return (NULL);
#ifdef RECEIVER
  ch = xml_get_text (xml, "Phineas.Receiver.Url");
  if (strstarts (url, ch))
  {
    if (url == req + 5)		/* this a POST?			*/
    {
      debug ("getting ebXML response\n");
      if ((ch = ebxml_process_req (xml, req)) != NULL)
        return (dbuf_setbuf (NULL, ch, strlen (ch)));
    }
    return (server_respond (200, "<h3>%s</h3>Receiver", Software));
  }
#endif
#ifdef CONSOLE
  ch = xml_get_text (xml, "Phineas.Console.Url");
  if (strstarts (url, ch))
    return (console_response (xml, req));
  if (strstarts (url, "/favicon.ico"))
  {
    char r[MAX_PATH];

    sprintf (r, "GET %s/images/favicon.ico HTTP/1.1\r\n\r\n", ch);
    return (console_response (xml, r));
  }
#endif
  return (server_respond (400, "404 - <bold>%s</bold> not found", url));
}

/*
 * set any needed header info
 */
int server_header (DBUF *b)
{
  char *ch,
       *status;
  int l,
      code = 200;
  char buf[120];

  /* find the end of the current header */
  ch = dbuf_getbuf (b);
  l = 0;
  while (1)
  {
    if (l > dbuf_size (b) - 2)
    {
      l = 0;
      break;
    }
    if (ch[l++] != '\n')
      continue;
    if (ch[l] == '\r')
      l++;
    if (ch[l] == '\n')
      break;
    if (strstarts (ch + l, "Status:"))
      code = atoi (ch + 8);
  }
  if (code < 300)
    status = "OK";
  else if (code < 500)
    status = "NOT FOUND";
  else
    status = "SERVER ERROR";
  l = sprintf (buf, "HTTP/1.1 %d %s\r\n%s",
    code, status, l ? "" : "\r\n");
  debug ("inserting header %s", buf);
  dbuf_insert (b, 0, buf, l);
  return (0);
}

void server_logrequest (NETCON *conn, int sz, char *req)
{
  int i;
  char buf[MAX_PATH];

  for (i = 0; i < sz; i++)
    if (req[i] == '\n')
      break;
  info ("%s: %.*s\n", net_remotehost (conn, buf), i, req);
}

/*
 * TASK to handle an incoming request
 */
int server_request (void *parm)
{
  SERVERPARM *s;
  DBUF *req, *res;

  s = (SERVERPARM *) parm;
  res = NULL;
  if ((req = server_receive (s->conn)) != NULL)
  {
    debug ("received %d bytes\n", dbuf_size (req));
    if (dbuf_size (req) == 0)
    {
      dbuf_free (req);
      net_close (s->conn);
      return (-1);
    }
    dbuf_putc (req, 0);
    server_logrequest (s->conn, dbuf_size (req), dbuf_getbuf (req));
    if ((res = server_response (s->xml, dbuf_getbuf (req))) == NULL)
    {
      res = server_respond (500,
	    "<h3>Failure processing ebXML request</h3>");
    }
    server_header (res);
    net_write (s->conn, dbuf_getbuf (res), dbuf_size (res));
    dbuf_free (res);
    dbuf_free (req);
  }
  net_close (s->conn);
  debug ("request completed\n");
  return (0);
}

/*
 * accept a connection and start a thread to manage it
 */
int server_accept (XML *xml, NETCON *conn, SSL_CTX *ctx, TASKQ *q)
{
  SERVERPARM *s;
  NETCON *c;

  debug ("accepting connection\n");
  if ((c = net_accept (conn, ctx)) == NULL)
  {
    error ("failed to successfully accept this socket\n");
    return (-1);
  }
  s = (SERVERPARM *) malloc (sizeof (SERVERPARM));
  s->conn = c;
  s->xml = xml;
  task_add (q, server_request, s);
  return (0);
}

/*
 * Listen for incoming connections until told to stop
 */
int server_listen (XML *xml, NETCON *conn, NETCON *ssl, SSL_CTX *ctx,
  int threads)
{
  char *ch;
  TASKQ *t;
  struct timeval timeout;
  fd_set fds;

  if ((conn == NULL) && (ssl == NULL))
    return (-1);

  t = task_allocq (threads, 2);
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;

  while (phineas_status () == 0)
  {
    FD_ZERO (&fds);
    if (conn != NULL)
      FD_SET (conn->sock, &fds);
    if (ssl != NULL)
      FD_SET (ssl->sock, &fds);
    if (select (2, &fds, NULL, NULL, &timeout) <= 0)
      continue;
    if ((conn != NULL) && FD_ISSET (conn->sock, &fds))
      server_accept (xml, conn, NULL, t);
    if ((ssl != NULL) && FD_ISSET (ssl->sock, &fds))
      server_accept (xml, ssl, ctx, t);
  }
  task_stop (t);
  task_freeq (t);
  return (0);
}


/*
 * A server thread (task)
 * assumes network is started and shutdown by parent thread
 */
server_task (XML *xml)
{
  SSL_CTX *ctx;
  NETCON *conn, *ssl;
  int port, threads, e;


  if ((threads = xml_get_int (xml, "Phineas.Server.NumThreads")) == 0)
    threads = 2;
  if (port = xml_get_int (xml, "Phineas.Server.Port"))
    conn = net_open ("ANY", port, threads, NULL);
  if (port = xml_get_int (xml, "Phineas.Server.SSL.Port"))
  {
    ctx = server_ctx (xml);
    ssl = net_open ("ANY", port, threads, ctx);
    if (conn != NULL)
      threads *= 2;
  }
  e = server_listen (xml, conn, ssl, ctx, threads);
  if (conn != NULL)
    net_close (conn);
  if (ssl != NULL)
  {
    net_close (ssl);
    if (ctx != NULL)
      SSL_CTX_free (ctx);
  }
  return (e);
}

#endif
