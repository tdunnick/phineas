/*
 * ebxml_sender.c
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
#define __SENDER__
#endif

#ifdef __SENDER__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "util.h"
#include "log.h"
#include "cfg.h"
#include "mime.h"
#include "basicauth.h"
#include "crypt.h"
#include "xcrypt.h"
#include "payload.h"
#include "ebxml.h"
#include "filter.h"


#ifndef debug
#define debug(fmt...)
#endif


/*********************** sender functions **************************/

#ifdef __SERVER__
DBUF *server_receive(NETCON *conn);
#define ebxml_receive server_receive
#else
/*
 * Receive a reply message
 */
DBUF *ebxml_receive (NETCON *conn)
{
  long n;
  int e;
  char c, *ch;
  DBUF *b;

  b = dbuf_alloc ();
  n = 0;
  /*
   * read to end of message request header - empty line
   */
  while ((e = net_read (conn, &c, 1)) == 1)
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
  if (e != 1)
  {
    if (e == 0)
      error ("Acknowledgment header read failed or connection closed\n");
    else
      error ("Timed out reading acknowledgment header\n");
    dbuf_free (b);
    return (NULL);
  }
  ch = strnstr (dbuf_getbuf (b), "Content-Length: ", dbuf_size (b));
  if (ch == NULL)
  {
    error ("Acknowledgment header missing Content-Length\n");
    dbuf_free (b);
    return (NULL);
  }
  n = atol (ch + 16);
  debug ("expecting %d bytes\n", n);

readbytes:

  while (n--)
  {
    if ((e = net_read (conn, &c, 1)) != 1)
    {
      if (e == 0)
        error ("Acknowledgment content read failed or connection closed");
      else
        error ("Timed out reading acknowledgment content\n");
      // note we'll take what we get and hope it's enough...
      break;
    }
    dbuf_putc (b, c);
  }
  if (n = net_available (conn))
  {
    warn ("Found %d unread bytes...\n", n);
    goto readbytes;
  }
  debug ("returning %d bytes\n", dbuf_size (b));
  return (b);
}
#endif

/*
 * Get the MESSAGEID from a queue row and use it to find
 * the map index and set the PID.
 * The MESSAGEID should be of the form Map.Name-PID.
 *
 * return map index or -1 if fails
 */

int ebxml_pid (XML *xml, QUEUEROW *r, char *pid)
{
  int n;
  char *ch;

  strcpy (pid, queue_field_get (r, "MESSAGEID"));
  if ((ch = strchr (pid, '-')) == NULL)
    return (-1);
  *ch++ = 0;
  n = cfg_map_index (xml, pid);
  strcpy (pid, ch);
  return (n);
}

/*
 * Build and return the mime payload container
 */
MIME *ebxml_getpayload (XML *xml, QUEUEROW *r)
{
  int l, mapi;
  XML *exml;
  MIME *msg;
  char *b,			/* buffer for payload		*/
       *type,
       *unc = NULL,		/* encryption info		*/
       *pw = NULL,
       dn[DNSZ],
       *organization,
       pid[MAX_PATH],
       buf[MAX_PATH],
       fname[MAX_PATH];

  debug ("getpayload container...\n");
  if ((mapi = ebxml_pid (xml, r, pid)) < 0)
    return (NULL);
  ppathf (fname, cfg_map (xml, mapi, "Processed"), "%s",
    queue_field_get (r, "PAYLOADFILE"));

  /* invoke the filter if given					*/
  b = cfg_map (xml, mapi, "Filter");
  if (*b)
  {
    char *emsg;
    DBUF *rbuf = dbuf_alloc ();

    debug ("filter read %s with %s\n", fname, b);
    if (filter_run (b, fname, NULL, NULL, rbuf, &emsg, cfg_timeout (xml)))
    {
      error ("Can't filter %s - %s\n", fname, strerror (errno));
      dbuf_free (rbuf);
      return (NULL);
    }
    if (*emsg)
      warn ("filter %s returned %s\n", b, emsg);
    free (emsg);
    l = dbuf_size (rbuf);
    b = dbuf_extract (rbuf);
  }
  else
  {
    debug ("reading data from %s\n", fname);
    if ((b = readfile (fname, &l)) == NULL)
    {
      error ("Can't read %s - %s\n", fname, strerror (errno));
      return (NULL);
    }
  }

  organization = cfg_org (xml);
  type = cfg_map (xml, mapi, "Encryption.Type");
  if ((type != NULL) && *type)	/* encrypted			*/
  {
    unc = cfg_map (xml, mapi, "Encryption.Unc");
    pw = cfg_map (xml, mapi, "Encryption.Password");
    strcpy (dn, cfg_map (xml, mapi, "Encryption.Id"));
  }

  msg = payload_create (b, l, fname, organization, unc, dn, pw);

  free (b);
  if (msg == NULL)
    error ("Can't create payload container for %s\n", fname);
  return (msg);
}

/*
 * Get the mime header (soap) container
 */
MIME *ebxml_getsoap (XML *xml, QUEUEROW *r)
{
  XML *soap;
  MIME *msg;
  char *ch,
       *pid,
       *partyid,
       *cpa,
       *organization,
       path[MAX_PATH],
       buf[MAX_PATH];
  int route;

  debug ("getting soap container...\n");
  organization = cfg_org (xml);
  pid = queue_field_get (r, "MESSAGEID");
  if (pid != NULL)
    pid = strchr (pid, '-');
  if (pid++ == NULL)
  {
    error ("Can't get PID from MESSAGEID\n");
    return (NULL);
  }
  if ((route = cfg_route_index (xml,  queue_field_get (r, "ROUTEINFO"))) 
    < 0) return (NULL);

  debug ("getting soap template for pid=%s org=%s...\n", pid, organization);
  if ((soap = ebxml_template (xml, XSOAP)) == NULL)
  {
    error ("Can't get SOAP template\n");
    return (NULL);
  }
  xml_set_text (soap, SOAPFROMPARTY, cfg_party (xml));
  partyid = "Someone_else";
  xml_set_text (soap, SOAPTOPARTY, partyid);
  cpa = cfg_route (xml, route, "Cpa");
  xml_set_text (soap, SOAPCPAID, cpa);
  xml_set_text (soap, SOAPCONVERSEID, pid);
  xml_set_text (soap, SOAPSERVICE, queue_field_get (r, "SERVICE"));
  xml_set_text (soap, SOAPACTION, queue_field_get (r, "ACTION"));
  sprintf (buf, "%ld@%s", pid, organization);
  xml_set_text (soap, SOAPMESSAGEID, buf);
  queue_field_set (r, "MESSAGECREATIONTIME", ptime (NULL, buf));
  xml_set_text (soap, SOAPDATATIME, buf);
  if (!strcmp (queue_field_get (r, "ACTION"), "Ping"))
  {
    xml_delete (soap, SOAPBODY);
  }
  else
  {
    sprintf (buf, "cid:%s@%s", queue_field_get (r, "PAYLOADFILE"),
      organization);
    debug ("set %s xlink:href=\"%s\"\n", SOAPMREF, buf);
    xml_set_attribute (soap, SOAPMREF, "xlink:href", buf);
    sprintf (buf, "%s.%s", r->queue->name, queue_field_get (r, "RECORDID"));
    xml_set_text (soap, SOAPDBRECID, buf);
    xml_set_text (soap, SOAPDBMESSID, queue_field_get (r, "MESSAGEID"));
    xml_set_text (soap, SOAPDBARGS, queue_field_get (r, "ARGUMENTS"));
    xml_set_text (soap, SOAPDBRECP, 
	queue_field_get (r, "MESSAGERECIPIENT"));
  }
  debug ("building soap mime container...\n");
  msg = mime_alloc ();
  mime_setHeader (msg, MIME_CONTENT, MIME_XML, 99);
  sprintf (buf, "<%s%s>", "ebxml-envelope@", organization);
  mime_setHeader (msg, MIME_CONTENTID, buf, 0);
  debug ("formatting soap xml...\n");
  ch = xml_format (soap);
  debug ("soap envelope:\n%s\n", ch);
  xml_free (soap);
  mime_setBody (msg, ch, strlen (ch));
  free (ch);
  debug ("returning soap message...\n");
  return (msg);
}

/*
 * Get the mime ebXML message
 */
MIME *ebxml_getmessage (XML *xml, QUEUEROW *r)
{
  MIME *msg, *payload, *soap;
  char *pid,
       *organization,
       buf[MAX_PATH];
  int route;

  if ((route = cfg_route_index (xml, queue_field_get (r, "ROUTEINFO"))) 
    < 0) return (NULL);
  organization = cfg_org (xml);
  pid = queue_field_get (r, "MESSAGEID");
  if (pid != NULL)
    pid = strchr (pid, '-');
  if (pid++ == NULL)
  {
    error ("Can't get PID from MESSAGEID\n");
    return (NULL);
  }
  if (strcmp (queue_field_get (r, "ACTION"), "Ping"))
  {
    if ((payload = ebxml_getpayload (xml, r)) == NULL)
      return (NULL);
  }
  else
    payload = NULL;
  if ((soap = ebxml_getsoap (xml, r)) == NULL)
  {
    mime_free (payload);
    return (NULL);
  }
  debug ("building multipart mime message\n");
  msg = mime_alloc ();
  sprintf (buf, "type=\"text/xml\"; start=\"ebxml-envelope@%s\";",
     organization);
  mime_setBoundary (msg, buf);
  sprintf (buf, "%s:%s",
    cfg_route (xml, route, "Host"),
    cfg_route (xml, route, "Port"));
  mime_setHeader (msg, "Host", buf, 99);
  mime_setHeader (msg, "Connection", "Close", 99);
  if (strcmp ("basic", 
    cfg_route (xml, route, "Authentication.Type")) == 0)
  {
    basicauth_request (buf, 
      cfg_route (xml, route, "Authentication.Id"),
      cfg_route (xml, route, "Authentication.Password"));
    pid = strchr (buf, ':');
    *pid++ = 0;
    while (isspace (*pid)) pid++;
    mime_setHeader (msg, buf, pid, 99);
  }
  mime_setHeader (msg, "SOAPAction", "\"ebXML\"", 99);
  mime_setMultiPart (msg, soap);
  if (payload != NULL)
    mime_setMultiPart (msg, payload);
  debug ("Completed multipart soap message\n");
  return (msg);
}

/*
 * queue a Ping request for this route
 */
int ebxml_qping (XML *xml, int route)
{
  QUEUE *q;
  QUEUEROW *r;
  int pl;
  char *ch,
  buf[MAX_PATH],
  pid[PTIMESZ];

  /*
   * queue it up
   */
  ch = cfg_route (xml, route, "Queue");
  debug ("queuing ping for %s\n", ch);
  if ((q = queue_find (ch)) == NULL)
  {
    error ("Can't find queue for %s\n", ch);
    return (-1);
  }
  ppid (pid);
  debug ("prepping new row\n");
  r = queue_row_alloc (q);
  sprintf (buf, "%s-%s", cfg_route (xml, route, "Name"), pid);
  queue_field_set (r, "MESSAGEID", buf);
  queue_field_set (r, "PAYLOADFILE", "");
  queue_field_set (r, "DESTINATIONFILENAME", "");
  queue_field_set (r, "ROUTEINFO", cfg_route (xml, route, "Name"));
  queue_field_set (r, "SERVICE", "urn:oasis:names:tc:ebxml-msg:service");
  queue_field_set (r, "ACTION", "Ping");
  queue_field_set (r, "ARGUMENTS", 
    cfg_route (xml, route, "Arguments"));
  queue_field_set (r, "MESSAGERECIPIENT",
  cfg_route (xml, route, "Recipient"));
  queue_field_set (r, "ENCRYPTION","no");
  queue_field_set (r, "SIGNATURE", "no");
  queue_field_set (r, "PUBLICKEYLDAPADDRESS", "");
  queue_field_set (r, "PUBLICKEYLDAPBASEDN", "");
  queue_field_set (r, "PUBLICKEYLDAPDN", "");
  queue_field_set (r, "CERTIFICATEURL","");
  queue_field_set (r, "PROCESSINGSTATUS", "queued");
  queue_field_set (r, "TRANSPORTSTATUS", "");
  queue_field_set (r, "PRIORITY", "0");
  debug ("pushing the queue\n");
  if (pl = queue_push (r) < 1)
  {
    error ("Failed queueing ping for %s\n", 
	cfg_route (xml, route, "Name"));
    pl = -1;
  }
  queue_row_free (r);
  info ("ebXML Ping for %s queueing completed\n",
    cfg_route (xml, route, "Name"));
  return (0);
}


/*
 * A folder polling processor for ebxml queues - register this with
 * the fpoller.
 *
 * This initializes and pushes a queue row.  Once queued it moves
 * the file to a processed point.
 *
 * xml - sender's configuration
 * prefix - xml path to this folder map
 * fname - file to be queued
 */
int ebxml_fprocessor (XML *xml, char *prefix, char *fname)
{
  struct stat st;
  QUEUE *q;
  QUEUEROW *r;
  char *ch;
  int pl;
  char qname[MAX_PATH],
       pid[PTIMESZ],
       buf[MAX_PATH];

  if (stat (fname, &st))
  {
    warn ("Can't access %s\n", fname);
    return (-1);
  }
  if (st.st_size == 0)
  {
    warn ("File %s empty... discarding\n", fname);
    unlink (fname);
    return (-1);
  }
  info ("Queuing ebXML folder %s for %s\n", fname, prefix);
  pl = strlen (prefix);
  /*
   * prep a file name
   */
  ppid (pid);
  sprintf (qname, "%s.%s", basename (fname), pid);

  /*
   * move file to processed folder
   */
  ppathf (buf, xml_getf (xml, "%sProcessed", prefix), "%s", qname);
  if (rename (fname, buf))
  {
    error ("Couldn't move %s to %s - %s\n", fname, buf, strerror (errno));
    return (-1);
  }

  /*
   * queue it up
   */
  ch = xml_getf (xml, "%sQueue", prefix);
  if ((q = queue_find (ch)) == NULL)
  {
    error ("Can't find queue for %s\n", ch);
    return (-1);
  }
  r = queue_row_alloc (q);
  sprintf (buf, "%s-%s", xml_getf (xml, "%sName", prefix), pid);
  queue_field_set (r, "MESSAGEID", buf);
  queue_field_set (r, "PAYLOADFILE", qname);
  queue_field_set (r, "DESTINATIONFILENAME", basename (fname));
  queue_field_set (r, "ROUTEINFO", xml_getf (xml, "%sRoute", prefix));
  queue_field_set (r, "SERVICE", xml_getf (xml, "%sService", prefix));
  queue_field_set (r, "ACTION", xml_getf (xml, "%sAction", prefix));
  queue_field_set (r, "ARGUMENTS", xml_getf (xml, "%sArguments", prefix));
  queue_field_set (r, "MESSAGERECIPIENT",
    xml_getf (xml, "%sRecipient", prefix));
  queue_field_set (r, "ENCRYPTION",
    *xml_getf (xml, "%sEncryption.Type", prefix) ? "yes" : "no");
  queue_field_set (r, "SIGNATURE", "no");
  queue_field_set (r, "PUBLICKEYLDAPADDRESS", "");
  queue_field_set (r, "PUBLICKEYLDAPBASEDN", "");
  queue_field_set (r, "PUBLICKEYLDAPDN", "");
  queue_field_set (r, "CERTIFICATEURL",
    xml_getf (xml, "%sEncryption.Unc", prefix));
  queue_field_set (r, "PROCESSINGSTATUS", "queued");
  queue_field_set (r, "TRANSPORTSTATUS", "");
  queue_field_set (r, "PRIORITY", "0");
  if (pl = queue_push (r) < 1)
  {
    error ("Failed queueing %s\n", fname);
    pl = -1;
  }
  queue_row_free (r);
  info ("ebXML folder %s for %s queueing completed\n", fname, prefix);
  return (pl);
}

/*
 * get an SSL context for this route
 */
SSL_CTX *ebxml_route_ctx (XML *xml, int route)
{
  char *id, *passwd, *unc, *ca,
       pathbuf[MAX_PATH * 2];

  debug ("getting SSL context for route %d\n", route);
  id = cfg_route (xml, route, "Protocol");
  if (stricmp (id, "https"))
    return (NULL);
  id = cfg_route (xml, route, "Authentication.Type");
  debug ("authentication type %s\n", id);
  if (strcmp ("certificate", id) == 0)
  {
    id = cfg_route (xml, route, "Authentication.Id");
    passwd = cfg_route (xml, route, "Authentication.Password");
    unc = pathf (pathbuf, "%s",
      cfg_route (xml, route, "Authentication.Unc"));
    debug ("unc path=%s\n", unc);
  }
  else 
  {
    if (*id)
    {
      error ("Authentication Type %s not supported\n", id);
      return (NULL);
    }
    id = NULL;
    passwd = NULL;
    unc = NULL;
  }
  if (*(ca = cfg_senderca (xml)))
    ca = pathf (pathbuf + MAX_PATH, "%s", ca);
  debug ("ca path=%s\n", ca);
  return (net_ctx (unc, unc, passwd, ca, 0));
}

/*
 * check the http status code for problems
 */
int ebxml_status (char *reply)
{
  char *ch, *nl;

  if ((nl = strchr (reply, '\n')) == NULL)
    nl = reply + strlen (reply);
  if (((ch = strchr (reply, ' ')) != NULL) && (ch < nl))
  {
    while (isspace (*ch)) ch++;
    if (atoi (ch) == 200)
      return (0);
  }
  else
    ch = reply;
  error ("EbXML reply failed: %.*s\n", nl - ch, ch);
  return (-1);
}

/*
 * parse a reply message and update the queue row with status
 */
int ebxml_parse_reply (char *reply, QUEUEROW *r)
{
  MIME *msg, *part;
  XML *xml;
  char *ch, buf[PTIMESZ];

  if (ebxml_status (reply))
    return (-1);
  if ((msg = mime_parse (reply)) == NULL)
  {
    error ("Failed parsing reply message\n");
    return (-1);
  }
  /* check the ebxml envelope for ack or error */
  if ((part = mime_getMultiPart (msg, 1)) == NULL)
  {
    error ("Reply missing ebxml envelope\n");
    mime_free (msg);
    return (-1);
  }
  if ((xml = xml_parse (mime_getBody (part))) == NULL)
  {
    error ("Failed parsing ebxml envelop\n");
    mime_free (msg);
    return (-1);
  }
  strcpy (buf, xml_get_text (xml, SOAPACTION));
  if (!strcmp (buf, "MessageError"))
  {
    debug ("Error reply received!\n");
    queue_field_set (r, "PROCESSINGSTATUS", "done");
    queue_field_set (r, "TRANSPORTSTATUS", "failed");
    ch = xml_get_attribute (xml, SOAPERROR, "eb:errorCode");
    debug ("%s eb:errorCode %s\n", SOAPERROR, ch);
    queue_field_set (r, "TRANSPORTERRORCODE", ch);
    queue_field_set (r, "APPLICATIONSTATUS", "not-set");
    queue_field_set (r, "APPLICATIONERRORCODE", "none");
    ch = xml_get_text (xml, SOAPERROR);
    debug ("SOAPERROR %s\n", ch);
    queue_field_set (r, "APPLICATIONRESPONSE", ch);
    queue_field_set (r, "MESSAGERECEIVEDTIME", ptime (NULL, buf));
    xml_free (xml);
    mime_free (msg);
    return (0);
  }
  xml_free (xml);

  /* Ping reply */
  if (strcmp (queue_field_get (r, "ACTION"), "Ping") == 0)
  {
    if (strcmp (buf, "Pong"))
    {
      error ("Expected 'Pong' action but got '%s'\n", buf);
      mime_free (msg);
      return (-1);
    }
    queue_field_set (r, "APPLICATIONSTATUS", "not-set");
    queue_field_set (r, "APPLICATIONERRORCODE", "none");
    queue_field_set (r, "APPLICATIONRESPONSE", "none");
    queue_field_set (r, "MESSAGERECEIVEDTIME", ptime (NULL, buf));
  }
  else /* regular reply */
  {
    if ((part = mime_getMultiPart (msg, 2)) == NULL)
    {
      error ("Reply missing status part\n");
      mime_free (msg);
      return (-1);
    }
    debug ("Body is...\n%s\n", mime_getBody (part));
    if ((xml = xml_parse (mime_getBody (part))) == NULL)
    {
      error ("Miss formatted Reply status\n");
      mime_free (msg);
      return (-1);
    }
    queue_field_set (r, "APPLICATIONSTATUS",
      xml_get_text (xml, "response.msh_response.status"));
    queue_field_set (r, "APPLICATIONERRORCODE",
      xml_get_text (xml, "response.msh_response.error"));
    queue_field_set (r, "APPLICATIONRESPONSE",
      xml_get_text (xml, "response.msh_response.appdata"));
    queue_field_set (r, "MESSAGERECEIVEDTIME", ptime (NULL, buf));
  
    /* TODO...
    queue_field_set (r, "RESPONSEMESSAGEID", "");
    queue_field_set (r, "RESPONSEARGUMENTS", "");
    queue_field_set (r, "RESPONSELOCALFILE", "");
    queue_field_set (r, "RESPONSEFILENAME", "");
    queue_field_set (r, "RESPONSEMESSAGEORIGIN", "");
    queue_field_set (r, "RESPONSEMESSAGESIGNATURE", "");
     */
    xml_free (xml);
  }
  queue_field_set (r, "PROCESSINGSTATUS", "done");
  queue_field_set (r, "TRANSPORTSTATUS", "success");
  queue_field_set (r, "TRANSPORTERRORCODE", "none");

  mime_free (msg);
  return (0);
}

/*
 * check a reply for a redirect, and if found set the new host, port,
 * and path to the redirected URL
 */
int ebxml_redirect (char *reply, char *host, int *port, char *path)
{
  char *ch;
  int v;

  /* get the response code 300-399 are redirects		*/
  if ((ch = strchr (reply, ' ')) == NULL)
    return (0);
  v = atoi (ch);
  if ((v < 300) || (v > 399))
    return (0);
  /* the location specifies a new URL				*/
  if ((ch = strstr (reply, "Location:")) == NULL)
    return (0);
  /* assume port 80 and parse the URL				*/
  ch += 10;
  *port = 80;
  if (!strncmp (ch, "https:", 6))
  {
    ch++;
    *port = 443;
  }
  /* parse out the host and optional port			*/
  ch += 7;
  while ((*host = *ch++) != '/')
  {
    if (*host == ':')
    {
      *port = atoi (ch);
      while (*++ch != '/');
      break;
    }
    host++;
  }
  *host = 0;
  /* finally parse out the path					*/
  while ((*path = *ch++) >= ' ') path++;
  *path = 0;
  return (1);
}

/*
 * send a message
 * return non-zero if message not sent successful with completed
 * queue info for status and transport
 */
int ebxml_send (XML*xml, QUEUEROW *r, MIME *msg)
{
  DBUF *b;
  NETCON *conn;
  char host[MAX_PATH];	/* need buffers for redirect		*/
  char path[MAX_PATH];
  int port, route, timeout, delay, retry;
  SSL_CTX *ctx;
  DBUF *b;
  char *rname, 		/* route name				*/
       *content, 	/* message content			*/
       buf[MAX_PATH];

  /* format up the message					*/
  if ((content = mime_format (msg)) == NULL)
  {
    queue_field_set (r, "PROCESSINGSTATUS", "done");
    queue_field_set (r, "TRANSPORTSTATUS", "failed");
    queue_field_set (r, "TRANSPORTERRORCODE", "failed formatting message");
    return (-1);
  }
  debug ("Send content:\n%s\n", content);

  /*
   * get connection info from the record route
   */
  if ((route = 
    cfg_route_index (xml, queue_field_get (r, "ROUTEINFO"))) < 0)
  {
    queue_field_set (r, "PROCESSINGSTATUS", "done");
    queue_field_set (r, "TRANSPORTSTATUS", "failed");
    queue_field_set (r, "TRANSPORTERRORCODE", "bad route");
    return (-1);
  }
  rname = cfg_route (xml, route, "Name");
  ctx = ebxml_route_ctx (xml, route);
  strcpy (host, cfg_route (xml, route, "Host"));
  port = atoi (cfg_route (xml, route, "Port"));
  if ((retry = atoi (cfg_route (xml, route, "Retry"))) == 0)
    retry = cfg_retries (xml);
  timeout = atoi (cfg_route (xml, route, "Timeout"));
  delay = cfg_delay (xml);
  strcpy (path, cfg_route (xml, route, "Path"));

sendmsg:

  info ("Sending ebXML %s:%d to %s\n", 
    r->queue->name, r->rowid, rname);
  debug ("opening connection socket on port=%d retrys=%d timeout=%d\n", 
    port, retry, timeout);
  if ((conn = net_open (host, port, 0, ctx)) == NULL)
  {
    error ("failed opening connection to %s:%d\n", host, port);
    goto retrysend;
  }
  				/* set read timeout if given	*/
  if (timeout)
  {
    net_timeout (conn, timeout * 1000);
    timeout <<= 1;		/* bump each try		*/
  }
  delay = 0;			/* connection OK, don't delay	*/
  queue_field_set (r, "MESSAGESENTTIME", ptime (NULL, buf));
  sprintf (buf, "POST %s HTTP/1.1\r\n", path);
  // ch = ebxml_beautify (ch);
  				/* all set... send the message	*/
  debug ("sending message...\n");
  net_write (conn, buf, strlen (buf));
  net_write (conn, content, strlen (content));
  debug ("reading response...\n");
  b = ebxml_receive (conn);
  debug ("closing socket...\n");
  net_close (conn);
  				/* no reply?			*/
  if (b == NULL)
  {
    warn ("Send response timed out or closed for %s\n", rname);

retrysend:			/* retry with a wait, or..	*/	
			
    if (retry-- && phineas_running ())
    {
      if (delay)
      {
	info ("Retrying send to %s in %d seconds\n", rname, delay);
        sleep (delay * 1000);
	delay <<= 1;
      }
      else			/* reset connection delay	*/
        delay = cfg_delay (xml);
      goto sendmsg;
    }
    if (ctx != NULL)		/* give up!			*/
      SSL_CTX_free (ctx);
    free (content);
    queue_field_set (r, "PROCESSINGSTATUS", "done");
    queue_field_set (r, "TRANSPORTSTATUS", "failed");
    queue_field_set (r, "TRANSPORTERRORCODE", "retries exhausted");
    return (-1);
  }
  debug ("reply was %d bytes\n%.*s\n", dbuf_size (b),
    dbuf_size (b), dbuf_getbuf (b));

  /*
   * handle redirects...
   * note this assumes the same SSL context should be used
   */
  if (ebxml_redirect (dbuf_getbuf (b), host, &port, path))
  {
    dbuf_free (b);
    goto sendmsg;
  }

  if (ctx != NULL)
    SSL_CTX_free (ctx);
  if (ebxml_parse_reply (dbuf_getbuf (b), r))
  {
    queue_field_set (r, "PROCESSINGSTATUS", "done");
    queue_field_set (r, "TRANSPORTSTATUS", "failed");
    queue_field_set (r, "TRANSPORTERRORCODE", "garbled reply");
  }
  debug ("send completed\n");
  dbuf_free (b);
  free (content);
  return (0);
}

/*
 * file the acknowledge for a queue entry
 */
int ebxml_file_ack (XML *xml, QUEUEROW *r)
{
  FILE *fp;
  int mapi;
  char *ch,
       buf[MAX_PATH];

  /* ping don't have folder maps, so no file ACK recorded	*/
  if (!strcmp (queue_field_get (r, "ACTION"), "Ping"))
    return (0);
  /* need our folder map ACK folder				*/
  if ((mapi = ebxml_pid (xml, r, buf)) < 0)
    return (-1);
  ch = cfg_map (xml, mapi, "Acknowledged");
  if (*ch == 0)
    return (0);		/* no ack recorded			*/

  ppathf (buf, ch, "%s", queue_field_get (r, "PAYLOADFILE"));
  if ((fp = fopen (buf, "w")) == NULL)
    return (0);
  fprintf (fp,
    "transportStatus=%s\n"
    "transportError=%s\n"
    "applicationStatus=%s\n"
    "applicationError=%s\n"
    "applicationData=%s\n"
    "responseMessageId=%s\n"
    "responseArguments=%s\n"
    "responseLocalFile=%s\n"
    "responseFileName=%s\n"
    "responseSignature=%s\n"
    "responseMessageOrigin=%s\n",
    queue_field_get (r, "TRANSPORTSTATUS"),
    queue_field_get (r, "TRANSPORTERRORCODE"),
    queue_field_get (r, "APPLICATIONSTATUS"),
    queue_field_get (r, "APPLICATIONERRORCODE"),
    queue_field_get (r, "APPLICATIONRESPONSE"),
    queue_field_get (r, "RESPONSEMESSAGEID"),
    queue_field_get (r, "RESPONSEARGUMENTS"),
    queue_field_get (r, "RESPONSELOCALFILE"),
    queue_field_get (r, "RESPONSEFILENAME"),
    queue_field_get (r, "RESPONSEMESSAGESIGNATURE"),
    queue_field_get (r, "RESPONSEMESSAGEORIGIN"));
  fclose (fp);
  return (0);
}

/*
 * A queue polling processor for ebxml queues - register this with
 * the qpoller.
 *
 * This builds an ebXML MIME message, opens a connection to a
 * receiver, sends the request, processes the response, and
 * finally updates the queue status.
 */
int ebxml_qprocessor (XML *xml, QUEUEROW *r)
{
  MIME *m;

  /*
   * build an ebXML MIME message
   */
  if ((m = ebxml_getmessage (xml, r)) == NULL)
  {
    char buf[24];
    queue_field_set (r, "MESSAGECREATIONTIME", ptime (NULL, buf));
    queue_field_set (r, "PROCESSINGSTATUS", "done");
    queue_field_set (r, "TRANSPORTSTATUS", "failed");
    queue_field_set (r, "TRANSPORTERRORCODE", "bad message");
    queue_push (r);
    return (-1);
  }
  /*
   * update the queue with message status
   */
  debug ("updating queue\n");
  queue_field_set (r, "PROCESSINGSTATUS", "waiting");
  queue_field_set (r, "TRANSPORTSTATUS", "attempted");
  queue_field_set (r, "TRANSPORTERRORCODE", "");
  queue_push (r);
  /*
   * send it to the destination
   */
  debug ("sending to destination\n");
  if (ebxml_send (xml, r, m) == 0)
    ebxml_file_ack (xml, r);
  /*
   * update the queue with status from the reply
   */
  debug ("updating reply\n");
  queue_push (r);
  /*
   * release all memory
   */
  mime_free (m);
  info ("ebXML %s:%d send completed\n", 
    r->queue->name, r->rowid);
  return (0);
}

#ifdef UNITTEST
#undef UNITTEST
#undef debug
#include "util.c"
#include "dbuf.c"
#include "log.c"
#include "xmln.c"
#include "xml.c"
#include "cfg.c"
#include "mime.c"
#include "queue.c"
#include "task.c"
#include "filter.c"
#include "fileq.c"
#include "b64.c"
#include "basicauth.c"
#include "crypt.c"
#include "xcrypt.c"
#include "find.c"
#include "fpoller.c"
#include "qpoller.c"
#include "net.c"
#include "payload.c"
#include "ebxml.c"

int ran = 0;
int phineas_running  ()
{
  return (ran++ < 3);
}

int main (int argc, char **argv)
{
  XML *xml;
  QUEUE *q;
  QUEUEROW *r;
  MIME *m;
  char *ch;
  int arg = 1;

  xml = xml_parse (PhineasConfig);
  loadpath (cfg_installdir (xml));
  queue_init (xml);

  /* test folder polling */
  if ((argc > arg) && (strcmp (argv[arg], "-f") == 0))
  {
    fpoller_register ("ebxml", ebxml_fprocessor);
    fpoller_task (xml);
    arg++;
    ran = 0;
  }
  /* test message from queue */
  if ((argc > arg) && (strcmp (argv[arg], "-m") == 0))
  {
    if ((q = queue_find ("MemSendQ")) == NULL)
      error ("can't find MemSendQ");
    else if ((r = queue_pop (q)) == NULL)
      error ("can't pop row\n");
    /*
    else if ((m = ebxml_getsoap (xml, r)) == NULL)
      error ("can't get soap container\n");
    */
    else if ((m = ebxml_getmessage (xml, r)) == NULL)
      error ("can't get message\n");
    else if ((ch = mime_format (m)) == NULL)
      error ("Can't format soap containter\n");
    else
      debug ("message MIME\n%s\n", ch);
    free (ch);
    mime_free (m);
    queue_row_free (r);
    arg++;
  }
  /* test queue polling */
  if ((argc > arg) && (strcmp (argv[arg], "-q") == 0))
  {
    qpoller_register ("EbXmlSndQ", ebxml_qprocessor);
    qpoller_task (xml);
    arg++;
    ran = 0;
  }
  queue_shutdown ();
  xml_free (xml);
  info ("%s %s\n", argv[0], Errors ? "failed" : "passed");
  exit (Errors);
}

#endif /* UNITTEST */
#endif /* __SENDER__ */
