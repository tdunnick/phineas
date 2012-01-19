/*
 * cpa.c
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
#include <stdlib.h>

#ifdef UNITTEST
#undef UNITTEST
#include "unittest.h"
#include "util.c"
#include "dbuf.c"
#include "xml.c"
#define UNITTEST
#define debug _DEBUG_
#else
#include "log.h"
#endif

#include "util.h"
#include "xml.h"

#ifndef debug
#define debug(fmt...)
#endif

/*
 * utility function for getting route data
 */
char *cpa_route_get (XML *xml, int route, char *tag)
{
  return (xml_getf (xml, "Phineas.Sender.RouteInfo.Route[%d].%s", 
    route, tag));
}

/*
 * convert route name to number
 */
int cpa_route (XML *xml, char *name)
{
  int i, n;

  n = xml_count (xml, "Phineas.Sender.RouteInfo.Route");
  for (i = 0; i < n; i++)
  {
    if (strcmp (cpa_route_get (xml, i, "Name"), name) == 0)
      return (i);
  }
  return (-1);
}

/*
 * Create a CPA for a new route and install it.
 */
int cpa_create (XML *xml, int route)
{
  XML *cpa;
  char buf[MAX_PATH];

  if (route < 0)
    return (-1);
  pathf (buf, "%s", xml_get_text (xml, "Phineas.CpaTemplate"));
  debug ("loading template %s\n", buf);
  if ((cpa = xml_load (buf)) == NULL)
  {
    error ("Can't load Sender CPA template %s\n", buf);
    return (-1);
  }
  debug ("configuring CPA\n");
  xml_set (cpa, 
    "tp:CollaborationProtocolAgreement.tp:PartyInfo[0].tp:PartyId",
      xml_get_text (xml, "Phineas.PartyId"));
  xml_set (cpa, 
    "tp:CollaborationProtocolAgreement.tp:PartyInfo[1].tp:PartyId",
     cpa_route_get (xml, route, "PartyId"));
  sprintf (buf, "%s:%s%s",
     cpa_route_get (xml, route, "Host"),
     cpa_route_get (xml, route, "Port"),
     cpa_route_get (xml, route, "Path"));
  debug ("uri=%s\n", buf);
  xml_set_attribute (cpa, 
    "tp:CollaborationProtocolAgreement.tp:PartyInfo[1]."
    "tp:Transport.tp:Endpoint", "tp:uri", buf);
  xml_set (cpa,
    "tp:CollaborationProtocolAgreement.tp:PartyInfo[1]."
    "tp:Transport.tp:TransportSecurity.tp:Protocol", 
     cpa_route_get (xml, route, "Protocol"));
  // TODO authentication parameters...
  pathf (buf, "%s%s.xml", 
    xml_get_text (xml, "Phineas.CpaDirectory"),
    cpa_route_get (xml, route, "Cpa"));
  debug ("Saving CPA to %s\n", buf);
  backup (buf);
  // write it out
  xml_beautify (cpa, 2);
  xml_save (cpa, buf);
  xml_free (cpa);
  return (0);
}

#ifdef UNITTEST

int main (int argc, char **argv)
{
  XML *xml;

  xml = xml_parse (PhineasConfig);
  cpa_create (xml, 0);
  xml_free (xml);
}

#elif CMDLINE
#include "util.c"
#include "log.c"
#include "dbuf.c"
#include "xml.c"

int main (int argc, char **argv)
{
  XML *xml;
  int route = 0;
  int i;

  LOGFILE = log_open (NULL);
  for (i = 1; i < argc; i++)
  {
    if (argv[i][0] == '-')
    {
      if (isdigit (argv[i][1]))
        route = atoi (argv[i] + 1);
      else
      {
	fprintf (stderr, "usage: %s [-route_#] [config]\n", argv[0]);
      }
    }
    else
    {
      xml = xml_load (argv[i]);
      cpa_create (xml, route);
      xml_free (xml);
    }
  }
}

#endif

