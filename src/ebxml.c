/*
 * ebxml.c
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
#include "util.h"
#include "dbuf.h"
#include "log.h"
#include "xml.h"
#include "mime.h"
#include "net.h"

#ifndef debug
#define debug(fmt...)
#endif

/*
 * Common soap envelope prefixes
 */
char
  *soap_ack = "soap-env:Envelope.soap-env:Header.eb:Acknowledgment.",
  *soap_hdr = "soap-env:Envelope.soap-env:Header.eb:MessageHeader.",
  *soap_bdy = "soap-env:Envelope.soap-env:Body.",
  *soap_manifest = "soap-env:Envelope.soap-env:Body.eb:Manifest.",
  *soap_dbinf = "soap-env:Envelope.soap-env:Body."
    "eb:Manifest.MetaData.DatabaseInfo.";

/************************ Shared Functions *************************/
/*
 * Get a field from configuration with prefix
 */
char *ebxml_get (XML *xml, char *prefix, char *name)
{
  char *ch, buf[MAX_PATH];

  strcpy (buf, prefix);
  strcat (buf, name);
  if ((ch = xml_get_text (xml, buf)) == NULL)
    ch = "";
  return (ch);
}

/*
 * Set a field from a configuration with prefix
 */
int ebxml_set (XML *xml, char *prefix, char *suffix, char *value)
{
  char buf[MAX_PATH];

  strcpy (buf, prefix);
  strcat (buf, suffix);
  return (xml_set_text (xml, buf, value) == NULL ? -1 : 0);
}

/*
 * Beautify and format an ebXML message.  Returns an allocated
 * message which the caller should free.
 */
char *ebxml_beautify (char *buf)
{
  char *ch;
  MIME *m, *p;
  XML *xml;

  if ((m = mime_parse (buf)) == NULL)
    error ("Failed multipart parse\n");
  if ((p = mime_getMultiPart (m, 1)) == NULL)
    error ("Failed getting part 1\n");
  if ((xml = xml_parse (mime_getBody (p))) == NULL)
    error ("Failed parsing XML part 1\n");
  xml_beautify (xml, 2);
  mime_setBody (p, ch = xml_format (xml), 0);
  free (ch);
  xml = xml_free (xml);
  if ((p = mime_getMultiPart (m, 2)) == NULL)
    error ("Failed getting part 2\n");
  if (((ch = mime_getHeader (p, MIME_CONTENT)) != NULL) &&
      (strstr (ch, MIME_XML) != NULL))
  {
    if ((xml = xml_parse (mime_getBody (p))) == NULL)
      error ("Failed parsing part 2 xml\n");
    xml_beautify (xml, 2);
    mime_setBody (p, ch = xml_format (xml), 0);
    free (ch);
  }
  ch = mime_format (m);
  mime_free (m);
  return (ch);
}

/*
 * Load, allocate, and return an xml template
 */
XML *ebxml_template (XML *xml, char *tag)
{
  char path[MAX_PATH];

  if (pathf (path, xml_get_text (xml, tag)) == NULL)
  {
    error ("Can't load template for %s\n", tag);
    return (NULL);
  }
  return (xml_load (path));
}

