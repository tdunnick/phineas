/*
 * basicauth.c
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

#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef UNITTEST
#undef UNITTEST
#define __SERVER__
#define __SENDER__
#define __RECEIVER__
#include "unittest.h"
#include "dbuf.c"
#include "util.c"
#include "xml.c"
#include "cpa.c"
#define debug _DEBUG_
#define UNITTEST
#endif

#include "log.h"
#include "basicauth.h"

#ifndef debug
#define debug(fmt,...)
#endif

/*
 * Check basic authentication.  Return zero if authenticated.
 * Return -1 if failed authentication.
 * Return 1 if authentication not attempted.
 */
int basicauth_check (XML *xml, char *path, char *req)
{
  int i, n;
  char *uid, *pw;
  char buf[DBUFSZ];

  debug ("request: %s\n", req);
  					/* is basic auth required?	*/
  if ((n = xml_count (xml, path)) < 1)
    return (0);
					/* get authentication header	*/
  uid = strstr (req, "Authorization: Basic ");
  if (uid == NULL)
    return (1);
  b64_decode (buf, uid + 21);
  uid = buf;
  if ((pw = strchr (uid, ':')) == NULL)
    return (1);
  *pw++ = 0;

  for (i = 0; i < n; i++)		/* check against users		*/
  {
    if ((strcmp (uid, xml_getf (xml, "%s%c%d%cUserID", 
      path, xml->indx_sep, i, xml->path_sep)) == 0) &&
     (strcmp (pw, xml_getf (xml, "%s%c%d%cPassword", 
      path, xml->indx_sep, i, xml->path_sep)) == 0)) 
      return (0);
  }
  return (-1);
}

/*
 * Send an authorization required response
 */
DBUF *basicauth_response (char *realm)
{
  DBUF *b = dbuf_alloc ();
  char *html = 
    "<html><body>Access restricted - "
    "Authorization required!</body></html>";

  dbuf_printf (b,
    "Status: 401\r\n"
    "WWW-Authenticate: Basic realm=\"%s\"\r\n"
    "Content-Length: %d\r\n\r\n%s", 
    realm, strlen (html), html);
  return (b);
}

/*
 * Add Basic authentication to the request buffer
 */
char *basicauth_request (char *b, char *uid, char *passwd)
{
  char *ch, buf[DBUFSZ];

  ch = buf + sprintf (buf, "%s:%s", uid, passwd) + 1;
  sprintf (b, "Authorization: Basic ");
  b64_encode (b + 21, buf);
  return (b);
}

