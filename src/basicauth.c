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

#ifdef UNITTEST
#include "unittest.h"
#define __SERVER__
#define __SENDER__
#define __RECEIVER__
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "log.h"
#include "b64.h"
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
  uid += 21;				/* limit b64 decoding		*/
  if ((pw = strchr (uid, '\n')) != NULL)
    *pw = 0;
  b64_decode (buf, uid);
  if (pw != NULL)			/* restore if limited		*/
    *pw = '\n';
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
 * Allocate and return an authorization required response
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
 * Create a basic authentication HTTP header in a buffer
 * b is the buffer location
 * uid and password are the authentication credentials
 */
char *basicauth_request (char *b, char *uid, char *passwd)
{
  char *ch, buf[DBUFSZ];

  ch = buf + sprintf (buf, "%s:%s", uid, passwd) + 1;
  sprintf (b, "Authorization: Basic ");
  b64_encode (b + 21, buf, strlen (buf), 0);
  return (b);
}

#ifdef UNITTEST
#undef UNITTEST
#undef debug
#include "dbuf.c"
#include "util.c"
#include "b64.c"
#include "xmln.c"
#include "xml.c"
#include "cpa.c"

int main (int argc, char **argv)
{
  warn ("No unit test performed\n");
  info ("%s %s\n", argv[0], Errors ? "failed" : "passed");
  exit (Errors);
}

#endif
