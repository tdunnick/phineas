/*
 * server.c
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
 *
 * Integrated HTTP server for use with stand-alone PHINEAS
 */

#ifdef UNITTEST
#define __SERVER__
#define phineas_fatal(msg...) fprintf(stderr,msg),exit(1)
#endif

#ifdef __SERVER__

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
    debug ("no header - %d read\n", dbuf_size (b));
    if (dbuf_size (b))
    {
      dbuf_printf (b, "%s", n == 1 ? "\r\n" : "\r\n\r\n");
      warn ("can't find end of request header: %.*s", 
	dbuf_size (b), dbuf_getbuf (b));
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
  char buf[4096];

  /*
   * note our response body is limited to 4096 bytes - only small
   * response is needed or allowed.
   */
  len = sprintf (buf, "<html><body>");
  va_start (ap, fmt);
  l = vsnprintf (buf + len, 4096-(len * 2 + 4), fmt, ap);
  va_end (ap);
  if (l > 0)
    len += l;
  len += sprintf (buf + len, "</body></html>");
  /*
   * if we are no longer running, notify the client that we are
   * closing this connection.
   */
  b = dbuf_alloc ();
  dbuf_printf (b, "Status: %d\r\n"
    "Content-Length: %d\r\n"
    "Connection: %s\r\n\r\n%s",
    code, len, phineas_running () ? "Keep-alive" : "Close", buf);
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
#ifdef __RECEIVER__
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
#ifdef __CONSOLE__
  ch = xml_get_text (xml, "Phineas.Console.Url");
  if (strstarts (url, ch) || strstarts (url, "/favicon.ico"))
    return (console_response (xml, req));
#endif
  if ((ch = strchr (url, '\n')) == NULL)
    ch = url + strlen (url);
  warn ("request not found for %.*s\n", ch - url, url);
  return (server_respond (400, "404 - <bold>%.*s</bold> not found", 
    ch - url, url));
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

  /* find the end of the current header and get return code */
  if (strstarts (ch = dbuf_getbuf (b), "Status:"))
    code = atoi (ch + 8);
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
      code = atoi (ch + l + 8);
  }
  if (code < 300)
    status = "OK";
  else if (code == 401)
    status = "Authorization Required";
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
  char *ch, buf[MAX_PATH];

  for (i = 0; i < sz; i++)
  {
    if (req[i] == '\n')
      break;
  }
  info ("%s: %.*s\n", net_remotehost (conn, buf), i, req);
}

/*
 * TASK to handle an incoming request
 */
int server_request (void *parm)
{
  SERVERPARM *s;
  DBUF *req, *res;
  char *curl;

  s = (SERVERPARM *) parm;
  curl = xml_get_text (s->xml, "Phineas.Console.Url");
  res = NULL;
  while ((req = server_receive (s->conn)) != NULL)
  {
    debug ("received %d bytes\n", dbuf_size (req));
    if (dbuf_size (req) == 0)
    {
      dbuf_free (req);
      net_close (s->conn);
      return (-1);
    }
    dbuf_putc (req, 0);
    /*
     * log the request, but filter out GET requests for the console... 
     * noise
     */
    if (!(*curl && strstarts (dbuf_getbuf (req) + 4, curl)))
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
    debug ("failed to successfully accept socket\n");
    return (-1);
  }
  s = (SERVERPARM *) malloc (sizeof (SERVERPARM));
  s->conn = c;
  s->xml = xml;
  if (!task_available (q))
    warn ("No available server threads for request\n");
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

  /*
   * Keep servicing requests until they stop coming in AND we are
   * no longer running.  This insures a nice shutdown, although a
   * naughty client could keep us from shutting down by flooding us
   * with requests.  We could add a counter here to prevent that.
   */
  while (1)
  {
    FD_ZERO (&fds);
    if (conn != NULL)
      FD_SET (conn->sock, &fds);
    if (ssl != NULL)
      FD_SET (ssl->sock, &fds);
    if (select (2, &fds, NULL, NULL, &timeout) <= 0)
    {
      if (phineas_running ())
        continue;
      break;
    }
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
  {
    if ((conn = net_open ("ANY", port, threads, NULL)) == NULL)
    {
      return (phineas_fatal ("Failed to open port %d\n", port));
    }
  }
  if (port = xml_get_int (xml, "Phineas.Server.SSL.Port"))
  {
    ctx = server_ctx (xml);
    if ((ssl = net_open ("ANY", port, threads, ctx)) == NULL)
    {
      if (conn != NULL)
        net_close (conn);
      return (phineas_fatal ("Failed to open SSL port %d\n", port));
    }
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
  if (e)
    phineas_fatal ("Failed to start PHINEAS server");
  return (0);
}

#endif
