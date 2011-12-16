/*
 * odbc.c
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

/*
 * Note that the SQL_SUCCEEDED macro apparently does not work well
 * when invoked directly on and SQL api call, probably due to int
 * conversions within the macro.  This may be compiler dependent, but
 * to insure correct operation, it is only invoked here on SQLRETURN
 * variables.
 */

#ifdef UNITTEST
#define __ODBCQ__
#endif

#ifdef __ODBCQ__

#include <stdio.h>
#include <windows.h>
#ifdef _GUID_DEFINED
#undef _GUID_DEFINED
#endif

#ifdef UNITTEST
#undef UNITTEST
#include "unittest.h"
#include "util.c"
#include "xml.c"
#include "dbuf.c"
#include "queue.c"
#define UNITTEST
#define debug _DEBUG_
#else
#include "log.h"
#endif

#include <sql.h>
#include <sqlext.h>
#include "util.h"
#include "queue.h"

/*
 * ODBC connection information
 */
typedef struct odbcq_conn
{
  SQLHENV env;
  SQLHDBC dbc;
  SQLHSTMT stmt;
} ODBCQCONN;

/*
 * Internal meta data
 */
typedef struct odbcq
{
  struct odbcq *next;
  ODBCQCONN *conn;			/* the connection 		*/
  int rowid;				/* max row number		*/
  int transport;			/* next transport row to pop	*/
  char *name;				/* queue name			*/
  signed char cmap[];			/* maps queue to table columns	*/
} ODBCQ;

ODBCQ *Odbcq = NULL;

#ifndef COLUMN_NAME
#define COLUMN_NAME 4
#endif

/*
 * get error information for a handle
 */
char *odbcq_error (char *dst, int sz, SQLHANDLE handle, SQLSMALLINT type)
{
  SQLSMALLINT i = 0;
  SQLINTEGER native;
  SQLRETURN ret;
  SQLCHAR state[7];
  SQLCHAR text[256];
  SQLSMALLINT l;
  int len = 0;

  *dst = 0;
  while (1)
  {
    ret = SQLGetDiagRec (type, handle, ++i, state, 
      &native, text, sizeof(text), &l);
    if (SQL_SUCCEEDED (ret))
    {
      len += snprintf (dst + len, sz - len,
	"\t%s:%ld:%ld:%s\n", state, i, native, text);
    }
    else 
    {
      switch (ret)
      {
	case SQL_INVALID_HANDLE : 
          snprintf (dst + len, sz - len, "\tinvalid handle!\n");
	  break;
	case SQL_ERROR :
	  snprintf (dst + len, sz - len, "\tasync operation incomplete!\n");
	  break;
	case SQL_NO_DATA :
	  if (i == 1)
	    snprintf (dst + len, sz - len, "\tno error data found!\n");
	  break;
	default :
	  snprintf (dst + len, sz - len, "\tunknown ret=%d\n", ret);
	  break;
      }
      break;
    }
    if (len >= sz)
      break;
  }
  return (dst);
}

/*
 * return a dB integer value
 */
int odbcq_getint (ODBCQCONN *c, char *fmt, ...)
{
  SQLINTEGER ind;
  SQLRETURN ret;
  int n = -1;
  va_list ap;
  char buf[80];

  va_start (ap, fmt);
  vsprintf (buf, fmt, ap);
  va_end (ap);
  debug ("executing %s\n", buf);
  ret = SQLExecDirect (c->stmt, buf, SQL_NTS);
  if (SQL_SUCCEEDED (ret))
  {
    ret = SQLFetch (c->stmt);
    if (SQL_SUCCEEDED (ret))
    {
      ret = SQLGetData (c->stmt, 1, SQL_C_CHAR, buf, sizeof (buf), &ind);
      if (SQL_SUCCEEDED (ret))
      {
	if (ind != SQL_NULL_DATA)
          n = atoi (buf);
      }
    }
  }
  if (!SQL_SUCCEEDED (ret))
  {
    char ebuf[1024];

    error ("Failed getting integer for statment: %s\n%s",
      buf, odbcq_error (ebuf, sizeof (ebuf), c->stmt, SQL_HANDLE_STMT));
  }
  SQLFreeStmt (c->stmt, SQL_CLOSE);
  return (n);
}

/*
 * Return our meta data for a queue
 */
ODBCQ *odbcq_find (QUEUE *q)
{
  ODBCQ *o;
  ODBCQCONN *c;
  SQLRETURN ret;
  int col, i;
  SQLINTEGER indicator;
  char *id, buf[1024];

  // debug ("looking for %s\n", q->name);
  for (o = Odbcq; o != NULL; o = o->next)
  {
    if (strcmp (q->name, o->name) == 0)
      return (o);
  }
  /*
   * none found... start by allocating some
   */
  debug ("%s meta data not found, allocating...\n", q->name);
  i = sizeof (ODBCQ) + strlen (q->name) + q->type->numfields + 1;
  o = (ODBCQ *) malloc (i);
  memset (o, 0, i);
  o->name = o->cmap + q->type->numfields;
  strcpy (o->name, q->name);
  /*
   * set up connection and statement
   */
  c = (ODBCQCONN *) q->conn->conn;
  o->conn = c;
  /*
   * set next row to pop and top row
   */
  id = q->type->field[0];
  o->rowid = odbcq_getint (c, "select max(%s) from %s", id, q->table);
  if (queue_field_find (q, "TRANSPORTSTATUS") > 0)
    o->transport = 0;
  else
    o->transport = -1;
  /*
   * map queue columns to the table columns
   */
  SQLSetStmtAttr (c->stmt, SQL_ATTR_MAX_ROWS , 0, 0);
  ret = SQLColumns (c->stmt, NULL, 0, NULL, 0, q->table, 
    strlen (q->table), NULL, 0);
  col = 1;
  while (SQL_SUCCEEDED (ret))
  {
    ret = SQLFetch (c->stmt);
    if (SQL_SUCCEEDED (ret))
    {
      ret = SQLGetData (c->stmt, COLUMN_NAME, SQL_C_CHAR,
        buf, sizeof (buf), &indicator);
      if (SQL_SUCCEEDED (ret) && (indicator != SQL_NULL_DATA))
      {
        if ((i = queue_field_find (q, buf)) >= 0)
          o->cmap[i] = col;
	else
	  warn ("failed to map field '%s' to %s\n", buf, q->type->name);
      }
      else
      {
        error ("Failed mapping at column %d\n%s", col,
	  odbcq_error (buf, 1024, c->stmt, SQL_HANDLE_STMT));
        o->rowid = o->transport = -1;	/* BAD queue!		*/
        break;
      }
      col++;
    }
  }
  debug ("rowid=%d transport=%d\n", o->rowid, o->transport);
  /*
   * clean up and return
   */
  SQLFreeStmt (c->stmt, SQL_CLOSE);
  o->next = Odbcq;
  Odbcq = o;
  return (o);
}

/*
 * return last row for a queue
 */
int odbcq_lastrow (QUEUE *q)
{
  ODBCQ *m;

  if ((m = odbcq_find (q)) == NULL)
    return (-1);
  return (m->rowid);
}

/*
 * return next row to pop
 */
int odbcq_transport (QUEUE *q)
{
  ODBCQ *m;
  char *k;
  int rowid;

  if ((m = odbcq_find (q)) == NULL)
    return (-1);
  if (m->transport < 0)
    return (-1);
  k = q->type->field[0];
  rowid = odbcq_getint ((ODBCQCONN *) q->conn->conn,
    "select min(%s) from %s where TRANSPORTSTATUS='queued' and "
    "%s>%d", k, q->table, k, m->transport);
  if (rowid > 0)
    return (m->transport = rowid);
  return (0);
}

/*
 * interface connection close
 */
int odbcq_close (void *conn)
{
  ODBCQCONN *c;
  ODBCQ **o, *f;

  c = (ODBCQCONN *) conn;
  /*
   * remove any meta data associated with this connection
   */
  o = &Odbcq;
  while ((f = *o) != NULL)
  {
    if (f->conn == c)
    {
      *o = f->next;
      free (f);
    }
    else
      o = &(f->next);
  }
  /*
   * shut down the connection
   */
  SQLFreeHandle (SQL_HANDLE_STMT, c->stmt);
  SQLDisconnect (c->dbc);		/* disconnect from driver */
  SQLFreeHandle (SQL_HANDLE_DBC, c->dbc);
  SQLFreeHandle (SQL_HANDLE_ENV, c->env);
  free (c);
  return (0);
}

/*
 * execute an arbitrary statement
 */
int odbcq_exec (ODBCQCONN *c, char *s)
{
  SQLRETURN ret;
  SQLSMALLINT col;
  char buf[256];
  int row = 0;

  SQLSetStmtAttr (c->stmt, SQL_ATTR_MAX_ROWS , (SQLPOINTER) 1, 0);
  /* exec the statement */
  ret = SQLExecDirect (c->stmt, s, SQL_NTS);
  SQLNumResultCols (c->stmt, &col);
  while (SQL_SUCCEEDED (ret))
  {
    SQLINTEGER ind;
    ret = SQLFetch (c->stmt);
    if (SQL_SUCCEEDED (ret))
    {
      ret = SQLGetData (c->stmt, col, SQL_C_CHAR, buf, sizeof (buf), &ind);
      if (SQL_SUCCEEDED (ret))
      {
        if (ind == SQL_NULL_DATA)
	  *buf = 0;
      }
      row++;
    }
  }
  SQLFreeStmt (c->stmt, SQL_CLOSE);
  return (row);
}

/*
 * quote strings with escapes returning length of output
 */
int odbcq_quote (char *dst, char *src)
{
  char *ch;

  ch = dst;
  *ch++ = '\'';
  while (*ch = *src++)
  {
    if (*ch++ == '\'')
      *ch++ = '\'';
  }
  *ch++ = '\'';
  *ch = 0;
  return (ch - dst);
}

/*
 * add or update a row and return the rowid
 */
int odbcq_push (QUEUEROW *r)
{
  ODBCQ *o;
  ODBCQCONN *c;
  SQLRETURN ret;
  char **field;
  char *s, buf[4096];
  int i, l, sz;

  /*
   * get the meta data
   */
  if ((o = odbcq_find (r->queue)) == NULL)
  {
    warn ("no meta data for %s\n", r->queue->name);
    return (-1);
  }

  l = 4096;
  sz = 0;
  debug ("pushing row %d for %s\n", r->rowid, r->queue->name);
  field = r->queue->type->field;
  if (r->rowid)				/* update		*/
  {
    for (i = 1; i < r->queue->type->numfields; i++)
      if (o->cmap[i] > 0) break;
    sz = sprintf (buf, "update %s set %s=", 
      r->queue->table, field[i]);
    sz += odbcq_quote (buf + sz, r->field[i]);
    while (++i < r->queue->type->numfields)
    {
      if ((o->cmap[i] > 0) && (r->field[i] != NULL))
      {
	sz += sprintf (buf + sz, ", %s=", field[i]);
	sz += odbcq_quote (buf + sz, r->field[i]);
      }
    }
    sz += sprintf (buf + sz, " where %s=%d", field[0], r->rowid);
  }
  else					/* insert		*/
  {
    if ((r->rowid = o->rowid + 1) < 1)
      r->rowid = 1;
    sz = sprintf (buf, "insert into %s (%s", r->queue->table, field[0]);
    for (i = 1; i < r->queue->type->numfields; i++)
    {
      if ((o->cmap[i] > 0) && (r->field[i] != NULL))
	sz += sprintf (buf + sz, ", %s", field[i]);
    }
    sz += sprintf (buf + sz, ") values (%d", r->rowid);
    for (i = 1; i < r->queue->type->numfields; i++)
    {
      if ((o->cmap[i] > 0) && (r->field[i] != NULL))
      {
	sz += sprintf (buf + sz, ", ");
        sz += odbcq_quote (buf + sz, r->field[i]);
      }
    }
    sz += sprintf (buf + sz, ")");
  }

  c = r->queue->conn->conn;
  SQLSetStmtAttr (c->stmt, SQL_ATTR_MAX_ROWS, (SQLPOINTER) 1, 0);
  /* exec the statement */
  ret = SQLExecDirect (c->stmt, buf, SQL_NTS);
  SQLFreeStmt (c->stmt, SQL_CLOSE);
  if (!SQL_SUCCEEDED (ret))
  {
    debug ("stmt: %s\n", buf);
    error ("push failed: %s", 
      odbcq_error (buf, sizeof (buf), c->stmt, SQL_HANDLE_STMT));
    return (-1);
  }
  if (r->rowid > o->rowid)
    o->rowid = r->rowid;
  return (r->rowid);
}

/*
 * get a specific row
 */
QUEUEROW *odbcq_get (QUEUE *q, int rowid)
{
  ODBCQ *m;
  ODBCQCONN *c;
  SQLRETURN ret;
  SQLSMALLINT col, i;
  SQLINTEGER ind;
  QUEUEROW *row = NULL;
  char buf[256];

  /*
   * get the meta data
   */
  if ((m = odbcq_find (q)) == NULL)
    return (NULL);

  /*
   * select the row
   */
  c = q->conn->conn;
  SQLSetStmtAttr (c->stmt, SQL_ATTR_MAX_ROWS, (SQLPOINTER) 1, 0);
  /* exec the statement */
  sprintf (buf, "select * from %s where %s = %d",
    q->table, q->type->field[0], rowid);
  ret = SQLExecDirect (c->stmt, buf, SQL_NTS);
  if (SQL_SUCCEEDED (ret))
  {
    ret = SQLFetch (c->stmt);
    if (SQL_SUCCEEDED (ret))
    {
      row = queue_row_alloc (q);
      row->rowid = rowid;
      for (i = 0; i < q->type->numfields; i++)
      {
	if ((col = m->cmap[i]) < 1)	/* column not mapped		*/
	{
	  debug ("%s not mapped\n", q->type->field[i]);
	  continue;
	}
        ret = SQLGetData (c->stmt, col, SQL_C_CHAR, buf, 
	  sizeof (buf), &ind);
        if (SQL_SUCCEEDED (ret))
        {
          if (ind == SQL_NULL_DATA)
	    *buf = 0;
	  row->field[i] = stralloc (row->field[i], buf);
	  // debug ("set %s=%s\n", q->type->field[i], buf);
        }
      }
    }
  }
  if (row == NULL)
  {
    warn ("failed reading %s row %d: %s", q->name, rowid,
      odbcq_error (buf, sizeof (buf), c->stmt, SQL_HANDLE_STMT));
  }
  SQLFreeStmt (c->stmt, SQL_CLOSE);
  return (row);
}

/*
 * get the next transport row, or last row if transport < 0
 */
QUEUEROW *odbcq_pop (QUEUE *q)
{
  int rowid;

  if ((rowid = odbcq_transport (q)) < 0)
    rowid = odbcq_lastrow (q);
  debug ("popping row %d\n", rowid);
  if (rowid == 0)
    return (NULL);
  return (odbcq_get (q, rowid));
}

/*
 * get the next row, call with rowid=0 to get first row
 */
QUEUEROW *odbcq_next (QUEUE *q, int rowid)
{
  int lastrow;
  QUEUEROW *r;

  lastrow = odbcq_lastrow (q);
  debug ("next row after %d up to %d\n", rowid, lastrow);
  while (rowid++ < lastrow)
  {
    if ((r = odbcq_get (q, rowid)) != NULL)
      return (r);
  }
  return (NULL);
}

/*
 * get the previous row, call with rowid=0 to get the last row
 */
QUEUEROW *odbcq_prev (QUEUE *q, int rowid)
{
  QUEUEROW *r;

  if (rowid == 0)
    rowid = odbcq_lastrow (q) + 1;
  debug ("prev row to %d\n", rowid);
  while (--rowid > 0)
  {
    if ((r = odbcq_get (q, rowid)) != NULL)
      return (r);
  }
  return (NULL);
}

/*
 * Makes a connection to a PHINMS Access dB using ODBC.
 * Connect unc expected to be of form "DSN=the_odbc_name".  Look it up
 * in your Administrators ODBC connection lists.  User and pass are
 * typically NULL and ignored here.
 */
int odbcq_connect (QUEUECONN *conn)
{
  ODBCQCONN *c;
  SQLCHAR inbuf[1024];
  SQLCHAR buf[1024];
  SQLSMALLINT len;

  c = (ODBCQCONN *) malloc (sizeof (ODBCQCONN));
  memset (c, 0, sizeof (ODBCQCONN));
  /* Allocate an environment handle */
  SQLAllocHandle (SQL_HANDLE_ENV, SQL_NULL_HANDLE, &c->env);
  /* We want ODBC 3 support */
  SQLSetEnvAttr (c->env, SQL_ATTR_ODBC_VERSION, (void *) SQL_OV_ODBC3, 0);
  /* Allocate a connection handle */
  SQLAllocHandle (SQL_HANDLE_DBC, c->env, &c->dbc);
  /* 
   * build a connection string 
   */
  if (*conn->unc)
  {
    char *ch;

    // look for a file DSN
    if (((ch = strstr (conn->unc, ".dsn")) != NULL) && (ch[4] == 0))
    {
      sprintf (inbuf, "FILEDSN=%s", conn->unc);
      len = 8 + fixpath (inbuf + 8);
      strcpy (inbuf + len, ";");
      len++;
    }
    else
    {
      len = sprintf (inbuf, "DSN=%s;", conn->unc);
    }
  }
  else
    len = 0;
  if (*conn->user)
    len += sprintf (inbuf + len, "UID=%s;", conn->user);
  if (*conn->passwd)
    len += sprintf (inbuf + len, "PWD=%s;", conn->passwd);
  if (*conn->driver)
    len += sprintf (inbuf + len, "DRIVER={%s};", conn->driver);
  /* Connect to the DSN mydsn */
  if (!SQL_SUCCEEDED (SQLDriverConnect (c->dbc, NULL, inbuf, SQL_NTS,
    buf, sizeof (buf), &len, SQL_DRIVER_COMPLETE)))
  {
    error ("SQLDriverConnect to %s:\n%s", 
      inbuf, odbcq_error (buf, sizeof (buf), c->dbc, SQL_HANDLE_DBC));
    odbcq_close (c);
    return (-1);
  }
  /* Allocate a statement handle */
  SQLAllocHandle (SQL_HANDLE_STMT, c->dbc, &c->stmt);

  debug ("conn=%x c=%x handle=%x\n", conn, c, c->dbc);
  buf[len] = 0;
  conn->conn = c;
  conn->close = odbcq_close;
  conn->push = odbcq_push;
  conn->pop = odbcq_pop;
  conn->get = odbcq_get;
  conn->nextrow = odbcq_next;
  conn->prevrow = odbcq_prev;
  info ("ODBC connected %s\n", buf);
  return (0);
}


#ifdef UNITTEST

void row_set (QUEUEROW *r, char *name, char *fmt, ...)
{
  char buf[256];
  va_list ap;

  va_start (ap, fmt);
  vsprintf (buf, fmt, ap);
  va_end (ap);
  queue_field_set (r, name, buf);
}

/*
 * random row for the receiver queue
 */
QUEUEROW *rand_rcv (QUEUEROW *r)
{
  char *names[] = { "data", "report", "out", "file", "dump" };
  char *service[] = { "elr", "hiv", "influenza" };
  char *action[] = { "save", "update", "forward", "notify" };
  char *emsg[] = { "none", "failed", "", "" };
  char *status[] = { "done", "failed", "queued", "in process" };
  char *statusa[] = { "ok", "failed", "waiting", "processing" };
  char *args[] = { "do=this", "don't=that", "config=none", "" };
  char *from[] = { "foo.com", "bar.state.edu", "some.lab.net", "joes.biz" };
  char *pname = names[rand()%5];
  char pbuf[PTIMESZ], pbuf2[PTIMESZ], pid[PTIMESZ];
  int ecode = rand()%4;
  time_t t, t2; time (&t); t -= rand()%20000; t2 = t + rand()%10000;

  ptime (&t, pbuf);
  ptime (&t2, pbuf2);

  r->rowid = 0;
  row_set (r, "MESSAGEID", "message_%d", rand ());
  row_set (r, "PAYLOADNAME", "%s", pname);
  row_set (r, "LOCALFILENAME", "local_%s", pname);
  row_set (r, "SERVICE", "%s", service[rand()%3]);
  row_set (r, "ACTION", "%s", action[rand()%4]);
  row_set (r, "ARGUMENTS", "%s", args[rand()%4]);
  row_set (r, "FROMPARTYID", "%s", from[rand()%4]);
  row_set (r, "MESSAGERECIPIENT", "%s", from[rand()%4]);
  row_set (r, "ERRORCODE", "%d", ecode);
  row_set (r, "ERRORMESSAGE", "%s", emsg[ecode]);
  row_set (r, "PROCESSINGSTATUS", "%s", status[ecode]);
  row_set (r, "APPLICATIONSTATUS", "%s", statusa[ecode]);
  row_set (r, "ENCRYPTION", "%s", rand()%1?"yes":"no");
  row_set (r, "RECEIVEDTIME", "%s", pbuf);
  row_set (r, "LASTUPDATETIME", "%s", pbuf2);
  row_set (r, "PROCESSID", "%s", ppid (pid));
  return (r);
}

/*
 * random row for the sender queue
 */
QUEUEROW *rand_trans (QUEUEROW *r)
{
  char *names[] = { "data", "report", "out", "file", "dump" };
  char *service[] = { "elr", "hiv", "influenza" };
  char *action[] = { "save", "update", "forward", "notify" };
  char *emsg[] = { "none", "failed", "", "" };
  char *status[] = { "done", "failed", "queued", "in process" };
  char *statusa[] = { "ok", "failed", "waiting", "processing" };
  char *args[] = { "do=this", "don't=that", "config=none", "" };
  char *from[] = { "foo.com", "bar.state.edu", "some.lab.net", "joes.biz" };
  char *pname = names[rand()%5];
  char *fromdn = from[rand()%4];
  char pbuf[PTIMESZ], pbuf2[PTIMESZ];
  int ecode = rand()%4;
  time_t t, t2; time (&t); t -= rand()%20000; t2 = t + rand()%10000;

  ptime (&t, pbuf);
  ptime (&t2, pbuf2);
  r->rowid = 0;
  row_set (r, "MESSAGEID", "message_%d" ,rand());
  row_set (r, "PAYLOADFILE", "%s", pname);
  row_set (r, "DESTINATIONFILENAME", "local_%s" ,pname);
  row_set (r, "ROUTEINFO", "route_%d" ,rand());
  row_set (r, "SERVICE", "%s", service[rand()%3]);
  row_set (r, "ACTION", "%s", action[rand()%4]);
  row_set (r, "ARGUMENTS", "%s", args[rand()%4]);
  row_set (r, "MESSAGERECIPIENT", "%s", fromdn);
  row_set (r, "MESSAGECREATIONTIME", "%s", pbuf2);
  row_set (r, "ENCRYPTION", "%s" ,rand()%1?"yes":"no");
  row_set (r, "SIGNATURE", "%s" ,rand()%1?"yes":"no");
  row_set (r, "PUBLICKEYLDAPADDRESS", "");
  row_set (r, "PUBLICKEYLDAPBASEDN", "DN=%s", fromdn);
  row_set (r, "PUBLICKEYLDAPDN", "");
  row_set (r, "CERTIFICATEURL", "URL=%s",fromdn);
  row_set (r, "PROCESSINGSTATUS", "%s", statusa[ecode]);
  row_set (r, "TRANSPORTSTATUS", "%s", status[ecode]);
  row_set (r, "TRANSPORTERRORCODE", "%d", ecode);
  row_set (r, "APPLICATIONSTATUS", "%s", status[ecode]);
  row_set (r, "APPLICATIONERRORCODE", "%d", ecode);
  row_set (r, "APPLICATIONRESPONSE", "%s", emsg[ecode]);
  row_set (r, "MESSAGESENTTIME", "%s", pbuf);
  row_set (r, "MESSAGERECEIVEDTIME", "%s", pbuf2);
  row_set (r, "RESPONSEMESSAGEID", "response_%d", rand());
  row_set (r, "RESPONSEARGUMENTS", "%s", args[rand()%4]);
  row_set (r, "RESPONSELOCALFILE", "%s", pname);
  row_set (r, "RESPONSEFILENAME", "%s", pname);
  row_set (r, "RESPONSEMESSAGEORIGIN", "URL=%s",fromdn);
  row_set (r, "RESPONSEMESSAGESIGNATURE", "%s", pname);
  row_set (r, "PRIORITY", "%d", rand()%4);
  return (r);
}

/*
 * initialize a queue with some test data
 */
int init_queue (QUEUE *q, QUEUEROW *(*fn)(QUEUEROW *r), int numrows)
{
  int i;
  QUEUEROW *r;

  debug ("initializing %s\n", q->name);
  r = queue_row_alloc (q);
  for (i = 0; i < numrows; i++)
  {
    fn (r);
    queue_push (r);
  }
  queue_row_free (r);
}

/*
 * build a pair of test queues
 */
int build_queues ()
{
  QUEUE *q;
  QUEUEROW *r;

  if ((q = queue_find ("AccessReceiveQ")) == NULL)
    fatal ("Couldn't find AccessReceiveQ\n");
  init_queue (q, rand_rcv, 100);
  if ((q = queue_find ("AccessSendQ")) == NULL)
    fatal ("Couldn't find AccessSendQ\n");
  init_queue (q, rand_trans, 100);
  return (0);
}

int dump_row (QUEUEROW *r)
{
  char *ch;

  if (r == NULL)
    return;
  ch = queue_field_get (r, "TRANSPORTSTATUS");
  if (ch == NULL)
    ch = "";
  debug ("Queue=%s Row=%d %s %s\n", r->queue->name, r->rowid, 
    queue_field_get (r, "MESSAGEID"), ch);
  free (r);
}

int main (int argc, char **argv)
{
  int i;
  XML *xml;
  QUEUE *q;
  QUEUETYPE *t;
  QUEUEROW *r = NULL;
  char *ch = NULL;
  char buf[4096];

  if ((xml = xml_parse (PhineasConfig)) == NULL)
    return (-1);
  loadpath (xml_get_text (xml, "Phineas.InstallDirectory"));
  debug ("Configuring...\n");
  if (queue_init (xml))
    fatal ("Couldn't initialize from config/Queues.xml!\n");
  if (argc > 1)
    build_queues ();
  q = queue_find ("AccessReceiveQ");
  if (q == NULL)
    fatal ("Couldn't find AccessReceiveQ\n");
  /*
  if (odbcq_find (q) == NULL)
    fatal ("Couldn't get ODBC queue info for %s\n", q->name);
  */
  if ((r = queue_pop (q)) == NULL)
    error ("Couldn't pop queue %s\n", q->name);
  else
    dump_row (r);
  i = r->rowid - 10;
  if ((r = queue_prev (q, i)) == NULL)
    error ("Couldn't get prev row %d queue %s\n", i, q->name);
  else
    dump_row (r);
  while ((r = queue_next (q, i)) != NULL)
  {
    i = r->rowid;
    dump_row (r);
  }
  q = queue_find ("AccessSendQ");
  if (q == NULL)
    fatal ("Couldn't find AccessSendQ\n");
  /*
  if (odbcq_find (q) == NULL)
    fatal ("Couldn't get ODBC queue info for %s\n", q->name);
    */
  if ((r = queue_pop (q)) == NULL)
    error ("Couldn't pop queue %s\n", q->name);
  else
    dump_row (r);
  while ((r = queue_pop (q)) != NULL)
  {
    i = r->rowid;
    dump_row (r);
  }
  if ((r = queue_prev (q, i)) == NULL)
    error ("Couldn't get prev row %d queue %s\n", i, q->name);
  else
    dump_row (r);
  debug ("Closing...\n");
  queue_shutdown ();
  debug ("Done!\n");
}

#endif /* UNITTEST */
#endif /* __ODBCQ__ */
