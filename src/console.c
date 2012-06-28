/*
 * console.c
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
#define __CONSOLE__
#endif

#ifdef __CONSOLE__

#include <stdio.h>

#ifdef UNITTEST
#undef UNITTEST
#include "unittest.h"
#include "dbuf.c"
#include "util.c"
#include "queue.c"
#include "fileq.c"
#include "xml.c"
#define debug _DEBUG_
#define UNITTEST
#endif

#include "dbuf.h"
#include "log.h"
#include "util.h"
#include "xml.h"
#include "queue.h"
#include "basicauth.h"

#ifndef debug
#define debug(fmt,...)
#endif

#define DISPLAYROWS 12
#define CONSOLEBODY "<div id='console'>"

/*
 * we won't use most of these, but here they are anyway...
 */
char *MimeTypes[][2] =
{
  {"application/envoy", "evy"},
  {"application/fractals", "fif"},
  {"application/futuresplash", "spl"},
  {"application/hta", "hta"},
  {"application/internet-property-stream", "acx"},
  {"application/mac-binhex40", "hqx"},
  {"application/msword", "doc"},
  {"application/msword", "dot"},
  {"application/octet-stream", "*"},
  {"application/octet-stream", "bin"},
  {"application/octet-stream", "class"},
  {"application/octet-stream", "dms"},
  {"application/octet-stream", "exe"},
  {"application/octet-stream", "lha"},
  {"application/octet-stream", "lzh"},
  {"application/oda", "oda"},
  {"application/olescript", "axs"},
  {"application/pdf", "pdf"},
  {"application/pics-rules", "prf"},
  {"application/pkcs10", "p10"},
  {"application/pkix-crl", "crl"},
  {"application/postscript", "ai"},
  {"application/postscript", "eps"},
  {"application/postscript", "ps"},
  {"application/rtf", "rtf"},
  {"application/set-payment-initiation", "setpay"},
  {"application/set-registration-initiation", "setreg"},
  {"application/vnd.ms-excel", "xla"},
  {"application/vnd.ms-excel", "xlc"},
  {"application/vnd.ms-excel", "xlm"},
  {"application/vnd.ms-excel", "xls"},
  {"application/vnd.ms-excel", "xlt"},
  {"application/vnd.ms-excel", "xlw"},
  {"application/vnd.ms-outlook", "msg"},
  {"application/vnd.ms-pkicertstore", "sst"},
  {"application/vnd.ms-pkiseccat", "cat"},
  {"application/vnd.ms-pkistl", "stl"},
  {"application/vnd.ms-powerpoint", "pot"},
  {"application/vnd.ms-powerpoint", "pps"},
  {"application/vnd.ms-powerpoint", "ppt"},
  {"application/vnd.ms-project", "mpp"},
  {"application/vnd.ms-works", "wcm"},
  {"application/vnd.ms-works", "wdb"},
  {"application/vnd.ms-works", "wks"},
  {"application/vnd.ms-works", "wps"},
  {"application/winhlp", "hlp"},
  {"application/x-bcpio", "bcpio"},
  {"application/x-cdf", "cdf"},
  {"application/x-compress", "z"},
  {"application/x-compressed", "tgz"},
  {"application/x-cpio", "cpio"},
  {"application/x-csh", "csh"},
  {"application/x-director", "dcr"},
  {"application/x-director", "dir"},
  {"application/x-director", "dxr"},
  {"application/x-dvi", "dvi"},
  {"application/x-gtar", "gtar"},
  {"application/x-gzip", "gz"},
  {"application/x-hdf", "hdf"},
  {"application/x-internet-signup", "ins"},
  {"application/x-internet-signup", "isp"},
  {"application/x-iphone", "iii"},
  {"application/x-javascript", "js"},
  {"application/x-latex", "latex"},
  {"application/x-msaccess", "mdb"},
  {"application/x-mscardfile", "crd"},
  {"application/x-msclip", "clp"},
  {"application/x-msdownload", "dll"},
  {"application/x-msmediaview", "m13"},
  {"application/x-msmediaview", "m14"},
  {"application/x-msmediaview", "mvb"},
  {"application/x-msmetafile", "wmf"},
  {"application/x-msmoney", "mny"},
  {"application/x-mspublisher", "pub"},
  {"application/x-msschedule", "scd"},
  {"application/x-msterminal", "trm"},
  {"application/x-mswrite", "wri"},
  {"application/x-netcdf", "cdf"},
  {"application/x-netcdf", "nc"},
  {"application/x-perfmon", "pma"},
  {"application/x-perfmon", "pmc"},
  {"application/x-perfmon", "pml"},
  {"application/x-perfmon", "pmr"},
  {"application/x-perfmon", "pmw"},
  {"application/x-pkcs12", "p12"},
  {"application/x-pkcs12", "pfx"},
  {"application/x-pkcs7-certificates", "p7b"},
  {"application/x-pkcs7-certificates", "spc"},
  {"application/x-pkcs7-certreqresp", "p7r"},
  {"application/x-pkcs7-mime", "p7c"},
  {"application/x-pkcs7-mime", "p7m"},
  {"application/x-pkcs7-signature", "p7s"},
  {"application/x-sh", "sh"},
  {"application/x-shar", "shar"},
  {"application/x-shockwave-flash", "swf"},
  {"application/x-stuffit", "sit"},
  {"application/x-sv4cpio", "sv4cpio"},
  {"application/x-sv4crc", "sv4crc"},
  {"application/x-tar", "tar"},
  {"application/x-tcl", "tcl"},
  {"application/x-tex", "tex"},
  {"application/x-texinfo", "texi"},
  {"application/x-texinfo", "texinfo"},
  {"application/x-troff", "roff"},
  {"application/x-troff", "t"},
  {"application/x-troff", "tr"},
  {"application/x-troff-man", "man"},
  {"application/x-troff-me", "me"},
  {"application/x-troff-ms", "ms"},
  {"application/x-ustar", "ustar"},
  {"application/x-wais-source", "src"},
  {"application/x-x509-ca-cert", "cer"},
  {"application/x-x509-ca-cert", "crt"},
  {"application/x-x509-ca-cert", "der"},
  {"application/ynd.ms-pkipko", "pko"},
  {"application/zip", "zip"},
  {"audio/basic", "au"},
  {"audio/basic", "snd"},
  {"audio/mid", "mid"},
  {"audio/mid", "rmi"},
  {"audio/mpeg", "mp3"},
  {"audio/x-aiff", "aif"},
  {"audio/x-aiff", "aifc"},
  {"audio/x-aiff", "aiff"},
  {"audio/x-mpegurl", "m3u"},
  {"audio/x-pn-realaudio", "ra"},
  {"audio/x-pn-realaudio", "ram"},
  {"audio/x-wav", "wav"},
  {"image/bmp", "bmp"},
  {"image/cis-cod", "cod"},
  {"image/gif", "gif"},
  {"image/ief", "ief"},
  {"image/jpeg", "jpe"},
  {"image/jpeg", "jpeg"},
  {"image/jpeg", "jpg"},
  {"image/pipeg", "jfif"},
  {"image/svg+xml", "svg"},
  {"image/tiff", "tif"},
  {"image/tiff", "tiff"},
  {"image/x-cmu-raster", "ras"},
  {"image/x-cmx", "cmx"},
  {"image/x-icon", "ico"},
  {"image/x-portable-anymap", "pnm"},
  {"image/x-portable-bitmap", "pbm"},
  {"image/x-portable-graymap", "pgm"},
  {"image/x-portable-pixmap", "ppm"},
  {"image/x-rgb", "rgb"},
  {"image/x-xbitmap", "xbm"},
  {"image/x-xpixmap", "xpm"},
  {"image/x-xwindowdump", "xwd"},
  {"message/rfc822", "mht"},
  {"message/rfc822", "mhtml"},
  {"message/rfc822", "nws"},
  {"text/css", "css"},
  {"text/h323", "323"},
  {"text/html", "htm"},
  {"text/html", "html"},
  {"text/html", "stm"},
  {"text/iuls", "uls"},
  {"text/plain", "bas"},
  {"text/plain", "c"},
  {"text/plain", "h"},
  {"text/plain", "txt"},
  {"text/richtext", "rtx"},
  {"text/scriptlet", "sct"},
  {"text/tab-separated-values", "tsv"},
  {"text/webviewhtml", "htt"},
  {"text/x-component", "htc"},
  {"text/x-setext", "etx"},
  {"text/x-vcard", "vcf"},
  {"video/mpeg", "mp2"},
  {"video/mpeg", "mpa"},
  {"video/mpeg", "mpe"},
  {"video/mpeg", "mpeg"},
  {"video/mpeg", "mpg"},
  {"video/mpeg", "mpv2"},
  {"video/quicktime", "mov"},
  {"video/quicktime", "qt"},
  {"video/x-la-asf", "lsf"},
  {"video/x-la-asf", "lsx"},
  {"video/x-ms-asf", "asf"},
  {"video/x-ms-asf", "asr"},
  {"video/x-ms-asf", "asx"},
  {"video/x-msvideo", "avi"},
  {"video/x-sgi-movie", "movie"},
  {"x-world/x-vrml", "flr"},
  {"x-world/x-vrml", "vrml"},
  {"x-world/x-vrml", "wrl"},
  {"x-world/x-vrml", "wrz"},
  {"x-world/x-vrml", "xaf"},
  {"x-world/x-vrml", "xof"},
  { NULL, NULL }
};

/*
 * status colors
 */
#define RC_OK "#99ff99"
#define RC_QUEUED "#f0f0f0"
#define RC_WAITING "#e0e0ff"
#define RC_FAILED "#ff5555"
#define RC_WARNING "#ffff99"

/*
 * move tags from one header to our page
 */
int console_headtag (DBUF *page, char *tag, char *head, int l)
{
  int p;
  char *st, *en, buf[MAX_PATH];

  debug ("checking for <%s> in <head>\n", tag);
  if ((st = strnstr (dbuf_getbuf (page), "<head>", l)) == NULL)
  {
    error ("<head> tag not found\n");
    return (0);
  }
  p = st - dbuf_getbuf (page) + 6;
  st = head;
  buf[0] = buf[1] = '<';
  strcpy (buf + 2, tag);
  while ((st = strnstr (st, buf + 1, l - (st - head))) != NULL)
  {
    buf[1] = '/';
    if ((en = strnstr (st, buf, l - (st - head))) == NULL) 
    {
      error ("missing </%s> tag\n", tag);
      return (-1);
    }
    en += strlen (tag) + 4;
    debug ("moving %.*s to head\n", en - st, st);
    dbuf_insert (page, p, st, en - st);
    p += en - st;
    st = en;
    buf[1] = '<';
  }
  return (0);
}

/*
 * get an html (fragment)
 */
DBUF *console_getHTML (DBUF *page, char *root, char *fname)
{
  int l;
  DBUF *b;
  char *st, *en, *data, path[MAX_PATH];

  pathf (path, "%s/%s", root, fname);
  if ((data = readfile (path, &l)) == NULL)
  {
    error ("Couldn't read %s\n", path);
    return (NULL);
  }
  debug ("checking for body\n");
  if ((st = strnstr (data, "<body>", l)) == NULL)
    return (dbuf_setbuf (NULL, data, l));
  console_headtag (page, "style", data, l - (st - data));
  console_headtag (page, "script", data, l - (st - data));
  if ((en = strnstr (st, "</body>", l - (st - data))) == NULL)
  {
    error ("Missing </body> tag in %s\n", fname);
    free (data);
    return (NULL);
  }
  debug ("copy to buf\n");
  st += 6;
  b = dbuf_alloc ();
  dbuf_write (b, st, en - st);
  debug ("built %s\n", dbuf_getbuf (b));
  free (data);
  debug ("returning buffer\n");
  return (b);
}

/*
 * Return the contents of a text file for the console div
 */
DBUF *console_textfile (char *fname)
{
  DBUF *b;
  FILE *fp;
  int c;

  if ((fp = fopen (fname, "r")) == NULL)
    return (NULL);
  b = dbuf_alloc ();
  dbuf_printf (b, "<pre>");
  while ((c = fgetc (fp)) > 0)
  {
    switch (c)
    {
      case '>' : dbuf_printf (b, "&gt;"); break;
      case '<' : dbuf_printf (b, "&lt;"); break;
      case '&' : dbuf_printf (b, "&amp;"); break;
      case '"' : dbuf_printf (b, "&quot;"); break;
      default : dbuf_putc (b, c); break;
    }
  }
  fclose (fp);
  dbuf_printf (b, "</pre>");
  debug ("read %d bytes from %s\n", dbuf_size (b), fname);
  return (b);
}

/*
 * load the requested page and return it along with any additional
 * header information needed. Modified to recognize /favicon.ico
 */

DBUF *console_file (XML *xml, char *uri)
{
  int c;
  FILE *fp;
  DBUF *b;
  char *ch, path[MAX_PATH];

  if (strcmp (uri, "/favicon.ico"))
    uri += strlen (xml_get_text (xml, "Phineas.Console.Url"));
  else
    uri = "/images/favicon.ico";
  if ((ch = strchr (uri, '?')) == NULL)
    ch = uri + strlen (uri);
  pathf (path, "%s%.*s", xml_get_text (xml, "Phineas.Console.Root"),
    ch - uri, uri);
  if ((fp = fopen (path, "rb")) == NULL)
  {
    error ("Can't open page %s\n", path);
    return (NULL);
  }
  b = dbuf_alloc ();
  while ((c = fgetc (fp)) >= 0)
    dbuf_putc (b, c);
  fclose (fp);
  debug ("read %d bytes from %s\n", dbuf_size (b), path);
  return (b);
}

/*
 * restart the console
 */
DBUF *console_restart ()
{
  extern int Status;
  DBUF *b = dbuf_alloc ();

  dbuf_printf (b, "<h3>Restarting...</h3>"
    "<script type=\"text/javascript\">\n"
    "document.getElementById ('restart').src = 'images/restart.gif';\n"
    "setTimeout ('window.history.back ()', 8000);\n"
    "</script\n>");
  phineas_restart ();
  return (b);
}

/*
 * return the status for a row
 */
char *console_getStatusColor (QUEUEROW *r)
{
  char *pstat, *ch;

  ch = queue_field_get (r, "TRANSPORTSTATUS");
  if (ch != NULL)		/* must be a sender		*/
  {
    pstat = queue_field_get (r, "PROCESSINGSTATUS");
    if (!strcmp (pstat, "queued"))
      return (RC_QUEUED);
    if (!strcmp (pstat, "waiting"))
      return (RC_WAITING);
    if (!strcmp (pstat, "done"))
    {
      if (strcmp (ch, "success"))
      {
        if (!strncmp (ch, "fail", 4))
          return (RC_FAILED);
        return (RC_WARNING);
      }
      ch = queue_field_get (r, "APPLICATIONERRORCODE");
      if (strcmp (ch, "none") && strcmp (ch, "InsertSucceeded"))
        return (RC_WARNING);
      return (RC_OK);
    }
  }
  /* must be a receiver					*/
  if ((ch = queue_field_get (r, "ERRORCODE")) != NULL)
  {
    if (strcmp (ch, "success") && strcmp (ch, "none"))
    {
      return (RC_FAILED);
    }
  }
  /* who knows...					*/
  return (RC_OK);
}


/*
 * set the needed header info
 */
int console_header (DBUF *b, char *path)
{
  int i, l;
  char *ch, buf[DBUFSZ];

  l = 0;
  if ((ch = strrchr (path, '.')) == NULL)
    ch = "txt";
  else
    ch++;
  for (i = 0; MimeTypes[i][0] != NULL; i++)
  {
    if (strcmp (ch, MimeTypes[i][1]) == 0)
    {
      l += sprintf (buf + l, "Content-Type: %s\r\n", MimeTypes[i][0]);
      break;
    }
  }
  /*
   * if we are shutting down, let the client know we are closing
   * this connection.
   */
  l += sprintf (buf + l, "Connection: %s\r\nContent-Length: %d\r\n\r\n", 
    phineas_running () ? "Keep-alive" : "Close", dbuf_size (b));
  debug ("inserting header:\n%s", buf);
  dbuf_insert (b, 0, buf, l);
  return (0);
}

/*
 * Build an HTML table with data for the indicated row
 */
DBUF *console_queue_row (QUEUE *q, int top, int rowid)
{
  int i;
  DBUF *b;
  QUEUEROW *r;
  char buf[DBUFSZ];

  debug ("getting row detail\n");
  b = dbuf_alloc ();
  if (q == NULL)
    return (b);
  if (rowid)
    r = queue_get (q, rowid);
  else
    r = queue_prev (q, top);
  if (r == NULL)
    return (b);
  dbuf_printf (b, 
    "<table id='qrow' cellpadding=0>"
    "<tr><td id='rowid'>%s %d</td><td>"
    "<a href='?queue=%s&top=%d&row=%d&delete'>"
    "<img src=\"images/delete.gif\">Delete</a>",
    q->type->field[0], r->rowid, q->name, top, r->rowid);
  if (!strcmp (q->type->name, "EbXmlSndQ"))
  {
    dbuf_printf (b, "<a href='?queue=%s&top=%d&row=%d&resend'>"
      "<img src=\"images/resend.gif\">ReSend</a>",
      q->name, top, r->rowid);
  }
  dbuf_printf (b,  "</td></tr><tr><td colspan=2><hr/></td></tr>");
  for (i = 1; i < q->type->numfields; i++)
  {
    if (r->field[i] == NULL)
      *buf = 0;
    else
      html_encode (buf, r->field[i]);
    dbuf_printf (b, "<tr><td><b>%s</b></td><td>%s</td></tr>",
      q->type->field[i], buf);
  }
  dbuf_printf (b, "</table>");
  return (b);
}

/*
 * Build an HTML table with 24 queue rows, beginning with rowid
 */
DBUF *console_queue (QUEUE *q, int top, int rowid)
{
  int row, id, i;
  DBUF *b;
  QUEUEROW *r;
  char buf[DBUFSZ];

  b = dbuf_alloc ();
  if (q == NULL)
  {
    debug ("NULL queue... no rows\n");
    dbuf_printf (b, "<h3>Queue not open</h3>");
    return (b);
  }
  debug ("getting set of rows for %s\n", q->name);
  if ((r = queue_prev (q, top)) == NULL)
  {
    debug ("no rows from %d for %s\n", top, q->name);
    if ((top == 0) || ((r = queue_prev (q, 0)) == NULL))
    {
      dbuf_printf (b, "<h3>No rows found for %s</h3>", q->name);
      return (b);
    }
    top = 0;
  }
  dbuf_printf (b,
    "<h2>%s</h2><div id='queue' >"
    "<table>"
    "<thead><tr>\n", q->name);
  for (i = 0; i < q->type->numfields; i++)
  {
    dbuf_printf (b, "<td>%s</td>", q->type->field[i]);
  }
  dbuf_printf (b, "</tr></thead>");
  row = 0;
  if (rowid == 0)
    rowid = atoi (r->field[0]);
  while (r != NULL)
  {
    id = atoi (r->field[0]);
    dbuf_printf (b, "<tr bgcolor='%s' %s >",
      console_getStatusColor (r),
      rowid == id ? " class='selected'" : "");
    dbuf_printf (b, "<td><a href='?queue=%s&top=%d&row=%d'>%d</a></td>",
      q->name, top, id, id);
    for (i = 1; i < q->type->numfields; i++)
    {
      if ((r->field[i] != NULL) &&
       (*html_encode (buf, r->field[i])))
      {
        dbuf_printf (b, "<td>%s</td>", buf);
      }
      else
        dbuf_printf (b, "<td>&nbsp;</td>");
    }
    dbuf_printf (b, "</tr>");
    r = queue_row_free (r);
    if (++row < DISPLAYROWS)
      r = queue_prev (q, id);
  }
  dbuf_printf (b, "</table></div>");
  if (row == DISPLAYROWS)
  {
    dbuf_printf (b, "<a href='?queue=%s&top=%d'>"
      "Previous Rows</a>&nbsp;&nbsp;", q->name, id);
  }
  if (top)
  {
    r = queue_prev (q, 0);
    id = atoi (r->field[0]);
    queue_row_free (r);
    if (top < id)
    {
      top += DISPLAYROWS;
      if (top > id)
	top = 0;
      dbuf_printf (b, "<a href='?queue=%s&top=%d'>Next Rows</a>",
        q->name, top);
    }
  }
  return (b);
}

/*
 * Build an HTML list of queues
 */
DBUF *console_queue_list (XML *xml)
{
  int i;
  DBUF *b;
  char *ch, buf[MAX_PATH];

  debug ("getting list of queues\n");
  if (xml == NULL)
    return (NULL);
  b = dbuf_alloc ();
  dbuf_printf (b, "<h3>Queues</h3><br>");
  debug ("ready with %s\n", dbuf_getbuf (b));
  for (i = 0; i < xml_count (xml, "Phineas.QueueInfo.Queue"); i++)
  {
    debug ("Queue %d\n", i);
    sprintf (buf, "Phineas.QueueInfo.Queue[%d].Name", i);
    debug ("looking for %s\n", buf);
    ch = xml_get_text (xml, buf);
    debug ("adding queue %s\n", ch);
    dbuf_printf (b, "<a href='?queue=%s'>%s</a><br>",
      ch, html_encode (buf, ch));
  }
  return (b);
}

/*
 * add select and submit buttons for CPA generation
 */
DBUF *console_ping (XML *xml)
{
  int i, n, l;
  char *ch, path[MAX_PATH];
  DBUF *b;

  b = dbuf_alloc ();
  dbuf_printf (b, "<form action='#' method='GET'>\n");

  l = sprintf (path,  "Phineas.Sender.RouteInfo.Route");
  n = xml_count (xml, path);
  if (n < 1)
    return (0);
  dbuf_printf (b, "<h3>Routes</h3>");
  for (i = 0; i < n; i++)
  {
    sprintf (path + l, "[%d].Name", i);
    ch = xml_get_text (xml, path);
    if (*ch == 0)
      continue;
    dbuf_printf (b,
      "<input type='radio' name='ping' value='%d' /> %s<br>\n", i, ch);
  }
  dbuf_printf (b, "<br><br>"
      "<input type='submit' name='submit' value='Ping Selected Route' />\n"
      "</form>");
  return (b);
}


/*
 * Return decoded parameter for this tag, or NULL if not found
 */
char *console_getParm (char *buf, char *parm, char *tag)
{
  int l;
  char *p, *d;

  if ((parm == NULL) || (tag == NULL) || (*tag == 0))
    return (NULL);
  p = parm;
  l = strlen (tag);
  while ((p = strstr (p, tag)) != NULL)
  {
    p += l;
    if (*p == '=')
    {
      d = buf;
      while (*++p && (*p != '&'))
        *d++ = *p;
      *d = 0;
      return (urldecode (buf));
    }
    else if ((*p == '&') || (*p <= ' '))
    {
      *buf = 0;
      return (buf);
    }
  }
  debug ("paramenter %s not found in '%s'\n", tag, parm);
  return (NULL);
}

int console_hasParm (char *parm, char *tag)
{
  char buf[MAX_PATH];

  return (console_getParm (buf, parm, tag) != NULL);
}

/*
 * Merge requested queue information into our page
 */
int console_qpage (DBUF *page, DBUF *queues, DBUF *rows, DBUF *detail)
{
  int offset;
  char *qp, *rp, *dp;
  DBUF *b;

  if ((qp = strstr (dbuf_getbuf (page), CONSOLEBODY)) == NULL)
  {
    warn ("Body %s not found in page\n", CONSOLEBODY);
    return (-1);
  }
  offset = qp + strlen (CONSOLEBODY) - dbuf_getbuf (page);
  qp = rp = dp = "&nbsp;";
  if (queues == NULL)
  {
    if (detail != NULL)
      dbuf_insert (page, offset, dbuf_getbuf (detail), dbuf_size (detail));
    return (0);
  }
  b = dbuf_alloc ();
  qp = dbuf_getbuf (queues);
  if (rows != NULL)
    rp = dbuf_getbuf (rows);
  if (detail != NULL)
    dp = dbuf_getbuf (detail);
  dbuf_printf (b, "<table id='qpage'><tr>"
    "<td id='qpagel'>%s<br>%s</td>"
    "<td id='qpager'>"
    "%s</td></tr></table>", rp, dp, qp);
  dbuf_insert (page, offset, dbuf_getbuf (b), dbuf_size (b));
  dbuf_free (b);
  return (0);
}

/*
 * insert the version codes
 */
int console_version (DBUF *b)
{
  int l;
  char *ch;
  extern char Software[];
  char vbuf[1024];

  ch = strstr (dbuf_getbuf (b), "<span id='version'>");
  if (ch == NULL)
  {
    warn ("Missing version span tags in console page");
    return (-1);
  }
  l = sprintf (vbuf, "<big><big><b>%s</b></big></big>", Software);
  dbuf_insert (b, ch - dbuf_getbuf (b), vbuf, l);
  return (0);
}

/*
 * copy the URI to dst and return the parameter part
 */
char *console_geturi (char *dst, char *req)
{
  char *ch, *r, *parm = NULL;

  if (req == NULL)
  {
    error ("NULL console request!\n");
    return (NULL);
  }
  if ((r = strchr (req, ' ')) == NULL)
  {
    error ("Invalid request %s\n", req);
    return (NULL);
  }
  r++;
  ch = dst;
  while (!isspace (*r))
  {
    if ((*ch++ = *r++) == '?')
    {
      parm = ch - 1;
      *parm++ = 0;
    }
  }
  *ch = 0;
  if (parm == NULL)
    parm = ch;
  return (parm);
}

/*
 * delete this row and return next one
 */
int console_deleterow (QUEUE *q, int rowid)
{
  QUEUEROW *r;
  info ("Deleting row %d from %s\n", rowid, q->name);
  r = queue_next (q, rowid);
  queue_delete (q, rowid);
  if (r != NULL)
    rowid = r->rowid;
  queue_row_free (r);
  return (rowid);
}

/*
 * resend this row
 */
int console_resendrow (QUEUE *q, int rowid)
{
  QUEUEROW *r;
  info ("Resending row %d from %s\n", rowid, q->name);
  if ((r = queue_get (q, rowid)) == NULL)
    return (0);
  queue_field_set (r, "PROCESSINGSTATUS", "queued");
  queue_push (r);
  queue_row_free (r);
  return (rowid);
}

/*
 * redirect a console request (for delete or resend)
 */
DBUF *console_redirect (char *uri, QUEUE *q, int top, int rowid)
{
  DBUF *d = dbuf_alloc ();
  dbuf_printf (d, "<script type='text/javascript'>"
    "window.location='%s?queue=%s&top=%d&row=%d'"
    "</script><h2>Working...</h2>"
    "<a href='%s?queue=%s&top=%d&row=%d'>Continue</a>",
    uri, q->name, top, rowid, uri, q->name, top, rowid);
  debug ("redirecting: %s\n", dbuf_getbuf (d));
  return (d);
}

/*
 * respond to a GET request
 */
DBUF *console_doGet (XML *xml, char *req)
{
  char *ch,
    uri[MAX_PATH],
    *parm,
    queue[MAX_PATH],
    buf[MAX_PATH];
  int top, rowid;
  QUEUE *q;
  DBUF *page = NULL,
       *queuelist = NULL,
       *rowlist = NULL,
       *rowdetail = NULL;
  extern char LogName[];
  DBUF *config_getConfig ();
  char *phineas_config ();

  if ((parm = console_geturi (uri, req)) == NULL)
    return (NULL);
  if ((page = console_file (xml, uri)) == NULL)
    return (NULL);
  if (strstr (uri, "console.html") == NULL)
  {
    debug ("returning non-console page for %s\n", uri);
    console_header (page, uri);
    return (page);
  }
  console_version (page);
  *queue = 0;
  rowid = top = 0;
  if (console_hasParm (parm, "help"))
  {
    rowdetail = console_getHTML (page,
      xml_get_text (xml, "Phineas.Console.Root"), "help.html");
  }
  else if (console_hasParm (parm, "log"))
  {
    rowdetail = console_textfile (LogName);
  }
  else if (console_hasParm (parm, "config"))
  {
    rowdetail = console_textfile (phineas_config ());
  }
  else if (console_hasParm (parm, "configure"))
  {
    rowdetail = config_getConfig ();
  }
  else if (console_hasParm (parm, "restart"))
  {
    rowdetail = console_restart ();
  }
#ifdef __SENDER__
  else if (console_getParm (buf, parm, "ping") != NULL)
  {
    if (isdigit (*buf) && ebxml_qping (xml, atoi (buf)))
      error ("Ping queueing failed\n");
    rowdetail = console_ping (xml);
  }
#endif
  else
  {
    console_getParm (queue, parm, "queue");
    if (*queue == 0)
      strcpy (queue, xml_get_text (xml, "Phineas.QueueInfo.Queue.Name"));
    debug ("queue=%s\n", queue);
    if (console_getParm (buf, parm, "row") != NULL)
      rowid = atoi (buf);
    if (console_getParm (buf, parm, "top") != NULL)
      top = atoi (buf);
    debug ("top=%d row=%d\n", top, rowid);
    q = queue_find (queue);

    /* 
     * delete and resend should redirect to fix up the browser URL,
     * otherwise, the refresh button doesn't work as expected.
     */

    if (console_hasParm (parm, "delete"))
    {
      rowid = console_deleterow (q, rowid);
      rowdetail = console_redirect (uri, q, top, rowid);
    }
#ifdef __SENDER__
    else if (console_hasParm (parm, "resend"))
    {
      console_resendrow (q, rowid);
      rowdetail = console_redirect (uri, q, top, rowid);
    }
#endif
    else
    {
      queuelist = console_queue_list (xml);
      rowlist = console_queue (q, top, rowid);
      rowdetail = console_queue_row (q, top, rowid);
    }
  }
  console_qpage (page, queuelist, rowlist, rowdetail);
  dbuf_free (queuelist);
  dbuf_free (rowlist);
  dbuf_free (rowdetail);
  console_header (page, uri);
  return (page);
}

/*
 * respond to a POST request
 */
DBUF *console_doPost (XML *xml, char *req)
{
  DBUF *config_setConfig (XML *, char *);

  // TOTO check for a configure request!
  return (config_setConfig (xml, req));
}

DBUF *console_response (XML *xml, char *req)
{
  if (basicauth_check (xml, "Phineas.Console.BasicAuth", req))
    return (basicauth_response ("Phineas Console"));
  if (strstarts (req, "POST "))
    return (console_doPost (xml, req));
  else
    return (console_doGet (xml, req));
}

#ifdef UNITTEST

int main (int argc, char **argv)
{
  XML *xml;
  DBUF *b;

  if ((xml = xml_parse (PhineasConfig)) == NULL)
    fatal ("Failed parsing PhineasConfig\n");
  loadpath (xml_get_text (xml, "Phineas.InstallDirectory"));
  xml_set_text (xml, "Phineas.QueueInfo.Queue[0].Table", "TransportQ.test");
  xml_set_text (xml, "Phineas.QueueInfo.Queue[1].Table", "ReceiveQ.test");
  queue_init (xml);
  b = console_doGet (xml, "/Phineas/console/console.html?", "");
  if (b == NULL)
    fatal ("Couldn't get console page\n");
  debug ("page saved to console/test.htm\n");
  writefile ("../console/test.htm", dbuf_getbuf (b), dbuf_size (b));
  dbuf_free (b);
  debug ("freeing xml\n");
   xml_free (xml);
   info ("%s unit test completed\n", argv[0]);
}

#endif /* UNITTEST */
#endif /* __CONSOLE__ */
