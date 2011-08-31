/*
 * config.c
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
#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef UNITTEST
#undef UNITTEST
#define SERVER
#define SENDER
#define RECEIVER
#include "unittest.h"
#include "dbuf.c"
#include "util.c"
#include "xml.c"
#include "cpa.c"
#define debug _DEBUG_
#define UNITTEST
char ConfigName[MAX_PATH];
#else
extern char ConfigName[];
#endif

#include "dbuf.h"
#include "log.h"
#include "util.h"
#include "xml.h"

#ifndef debug
#define debug(fmt,...)
#endif

/*
 * current configuration in edit
 */
char EditConfig[MAX_PATH] = "";

/*
 * tag types
 */
#define TAG_TEXT 1
#define TAG_PASSWD 2
#define TAG_FILE 3
#define TAG_DIR 4
#define TAG_REF 5
#define TAG_OPT 6
#define TAG_FIELD 7

/*
 * default tag widths
 */
#define FILE_WIDTH 50
#define PORT_WIDTH 4
#define DIR_WIDTH 50
#define PASS_WIDTH 14

/*
 * tags select option lists
 */
char *OptProto[] = { "", "http", "https", NULL };
char *OptLogLevel[] = { "debug", "info", "warn", "error", "none", NULL };
char *OptAuth[] = { "", "basic", "custom", "sdn", "certificate", NULL };
char *OptProcessor[] = { "", "ebxml", NULL };
char *OptEncryption[] = { "", "certificate", "LDAP", NULL };
/*
 * lists of xml configuration tags used
 */

typedef struct
{
  char *tag;
  char type, width;
  void *ref;
} CTAG;

CTAG GeneralTags[] =
{
  { "InstallDirectory", TAG_DIR, DIR_WIDTH, NULL },
  { "PartyId", TAG_TEXT, 24, NULL },
  { "Organization", TAG_TEXT, 44, NULL },
  { "SoapTemplate", TAG_FILE, FILE_WIDTH, NULL },
  { "EncryptionTemplate", TAG_FILE, FILE_WIDTH, NULL },
  { "CPATemplate", TAG_FILE, FILE_WIDTH, NULL },
  { "CpaDirectory", TAG_DIR, DIR_WIDTH, NULL },
  { "TempDirectory", TAG_DIR, DIR_WIDTH, NULL },
  { "LogFile", TAG_FILE, FILE_WIDTH, NULL },
  { "LogLevel", TAG_OPT, 10, OptLogLevel },
#ifdef SERVER
  { "Server.Port", TAG_TEXT, 4, NULL },
  { "Server.NumThreads", TAG_TEXT, 4, NULL },
#endif
  { "Console.Url", TAG_TEXT, 44, NULL },
  { "Console.Root", TAG_TEXT, 44, NULL },
#ifdef RECEIVER
  { "Receiver.Url", TAG_TEXT, 44, NULL },
#endif
  { NULL, 0, 0, NULL }
};

#ifdef SERVER
CTAG SSLTags[] =
{
  { "Port", TAG_TEXT, 4, NULL },
  { "CertFile", TAG_FILE, FILE_WIDTH, NULL },
  { "KeyFile", TAG_FILE, FILE_WIDTH, NULL },
  { "Password", TAG_PASSWD, PASS_WIDTH, NULL },
  { "AuthFile", TAG_FILE, FILE_WIDTH, NULL },
  { NULL, 0, 0, NULL }
};
#endif

CTAG QTypeTags[] =
{
  { "Name", TAG_TEXT, 24, NULL },
  { "Field", TAG_FIELD, 24, NULL },
  { NULL, 0, 0, NULL }
};

CTAG ConnectionTags[] =
{
  { "Name", TAG_TEXT, 24, NULL },
  { "Id", TAG_TEXT, 24, NULL },
  { "Password", TAG_PASSWD, PASS_WIDTH, NULL },
  { "Unc", TAG_TEXT, 24, NULL },
  { NULL, 0, 0, NULL }
};

CTAG QueueTags[] =
{
  { "Name", TAG_TEXT, 24, NULL },
  { "Type", TAG_REF, 24, "Phineas.QueueInfo.Type" },
  { "Connection", TAG_REF, 24, "Phineas.QueueInfo.Connection" },
  { "Table", TAG_TEXT, 24, NULL },
  { NULL, 0, 0, NULL }
};

#ifdef SENDER
CTAG SenderTags[] =
{
  { "PollInterval", TAG_TEXT, 24, NULL },
  { "MaxThreads", TAG_TEXT, 24, NULL },
  { "CertificateAuthority", TAG_FILE, FILE_WIDTH, NULL },
  { "MaxRetry", TAG_TEXT, 24, NULL },
  { "DelayRetry", TAG_TEXT, 24, NULL },
  { NULL, 0, 0, NULL }
};

CTAG RouteTags[] =
{
  { "Name", TAG_TEXT, 24, NULL },
  { "PartyId", TAG_TEXT, 24, NULL },
  { "Cpa", TAG_TEXT, 24, NULL },
  { "Host", TAG_TEXT, 24, NULL },
  { "Path", TAG_TEXT, 24, NULL },
  { "Port", TAG_TEXT, 24, NULL },
  { "Protocol", TAG_OPT, 24, OptProto },
  { "Timeout", TAG_TEXT, 24, NULL },
  { "Retry", TAG_TEXT, 24, NULL },
  { "Authentication.Type", TAG_OPT, 24, OptAuth },
  { "Authentication.Id", TAG_TEXT, 24, NULL },
  { "Authentication.Password", TAG_PASSWD, PASS_WIDTH, NULL },
  { "Authentication.Unc", TAG_TEXT, 24, NULL },
  { "Queue", TAG_REF, 24, "Phineas.QueueInfo.Queue" },
 { NULL, 0, 0, NULL }
};

CTAG MapTags[] =
{
  { "Name", TAG_TEXT, 24, NULL },
  { "Queue", TAG_REF, 24, "Phineas.QueueInfo.Queue" },
  { "Processor", TAG_OPT, 24, OptProcessor },
  { "Filter", TAG_TEXT, 24, NULL },
  { "Folder", TAG_DIR, DIR_WIDTH, NULL },
  { "Processed", TAG_DIR, DIR_WIDTH, NULL },
  { "Acknowledged", TAG_DIR, DIR_WIDTH, NULL },
  { "Route", TAG_REF, 24, "Phineas.Sender.RouteInfo.Route" },
  { "Service", TAG_TEXT, 24, NULL },
  { "Action", TAG_TEXT, 24, NULL },
  { "Arguments", TAG_TEXT, 24, NULL },
  { "Recipient", TAG_TEXT, 24, NULL },
  { "Encryption.Type", TAG_OPT, 24, OptEncryption },
  { "Encryption.Id", TAG_TEXT, 24, NULL },
  { "Encryption.Password", TAG_TEXT, 24, NULL },
  { "Encryption.Unc", TAG_TEXT, 24, NULL },
  { NULL, 0, 0, NULL }
};

#endif

#ifdef RECEIVER
CTAG ServiceTags[] =
{
  { "Name", TAG_TEXT, 24, NULL },
  { "Directory", TAG_DIR, DIR_WIDTH, NULL },
  { "Queue", TAG_REF, 24, "Phineas.QueueInfo.Queue" },
  { "Filter", TAG_TEXT, 24, NULL },
  { "Cpa", TAG_TEXT, 24, NULL },
  { "Service", TAG_TEXT, 24, NULL },
  { "Action", TAG_TEXT, 24, NULL },
  { "Arguments", TAG_TEXT, 80, NULL },
  { "Encryption.Type", TAG_OPT, 24, OptEncryption },
  { "Encryption.Id", TAG_TEXT, 24, NULL },
  { "Encryption.Password", TAG_TEXT, 24, NULL },
  { "Encryption.Unc", TAG_TEXT, 24, NULL },
  { NULL, 0, 0, NULL }
};

#endif

struct
{
  char *name;
  char *prefix;
  CTAG *tags;
  int repeats;
} Tabs[] =
{
  { "General", "Phineas", GeneralTags, 0 },
#ifdef SERVER
  { "SSL", "Phineas.Server.SSL", SSLTags, 0 },
#endif
#ifdef RECEIVER
  { "Services", "Phineas.Receiver.MapInfo.Map", ServiceTags, 1 },
#endif
#ifdef SENDER
  { "Sender", "Phineas.Sender", SenderTags, 0 },
  { "Routes", "Phineas.Sender.RouteInfo.Route", RouteTags, 1 },
  { "Maps", "Phineas.Sender.MapInfo.Map", MapTags, 1 },
#endif
  { "Q Types", "Phineas.QueueInfo.Type", QTypeTags, 1 },
  { "Connections", "Phineas.QueueInfo.Connection", ConnectionTags, 1 },
  { "Queues", "Phineas.QueueInfo.Queue", QueueTags, 1 },
  { NULL, NULL }
};

/*
 * construct a select list from a tab separated list of values
 */

int config_select (char *name, char *path, char *val, char **list, DBUF *b)
{
  char *ch;

  dbuf_printf (b, "<tr><td>%s</td><td><select size='1' name='%s'>\n",
    name, path);
  while (*list != NULL)
  {
    dbuf_printf (b, "<option %svalue='%s'>%s</option>\n",
      strcmp (val, *list) ? "" : "selected ", *list,
      **list ? *list : "(none selected)");
    list++;
  }
  dbuf_printf (b, "</select></td></tr>\n");
  return (0);
}

/*
 * return select list of references
 */
int config_reference (XML *xml, char *name, char *path,
  char *value, char *tag, DBUF *b)
{
  int i, n;
  char p[MAX_PATH];
  char *list[100];

  debug ("referencing %s\n", tag);
  n = xml_count (xml, tag);
  debug ("%d options referenced\n", n);
  if (n > 100)
    error ("too many %s tags for configuration\n", tag);
  list[0] = "";
  for (i = 0; i < n; i++)
  {
    sprintf (p, "%s[%d].Name", tag, i);
    list[i + 1] = xml_get_text (xml, p);
    debug ("got %s for %s\n", list[i + 1], p);
  }
  list[i + 1] = NULL;
  debug ("selecting for %s\n", value);
  return (config_select (name, path, value, list, b));
}

/*
 * add all the rows for each (repeated) tag to the form
 */
int config_getRows (XML *xml, char *prefix, CTAG *tag, int new, DBUF *b)
{
  int l, i, n;
  char *ch,
       name[MAX_PATH],
       path[MAX_PATH];

  if (xml == NULL)
  {
    error ("NULL configuration\n");
    return (-1);
  }
  debug ("prefix %s\n", prefix);
  dbuf_printf (b, "<table border=0 summary='%s'>\n", prefix);
  while (tag->tag != NULL)
  {
    l = sprintf (path, "%s.%s", prefix, tag->tag);
    debug ("path %s\n", path);
    if ((n = xml_count (xml, path)) == 0)
      n = 1;
    debug ("getting %d values for %s\n", n, path);
    for (i = 0; i < n; i++)
    {
      if ((n > 1) || (tag->type == TAG_FIELD))
      {
        sprintf (path + l, "[%d]", i);
	sprintf (name, "%s %d ", tag->tag, i + 1);
      }
      else
      {
	strcpy (name, tag->tag);
      }
      for (ch = strchr (name, '.'); ch != NULL; ch = strchr (ch, '.'))
	*ch++ = ' ';
      if (new || ((ch = xml_get_text (xml, path)) == NULL))
	ch = "";
      switch (tag->type)
      {
        case TAG_REF :
	  config_reference (xml, name, path, ch, tag->ref, b);
	  break;
        case TAG_OPT :
	  config_select (name, path, ch, tag->ref, b);
	  break;
	case TAG_PASSWD :
          dbuf_printf (b, "<tr><td>%s</td><td><input type='password'"
            "name='%s' value='%s' size='%d'/></td></tr>\n",
	    name, path, ch, tag->width);
	  break;
	case TAG_FIELD :
        case TAG_FILE :
	case TAG_TEXT :
	default :
          dbuf_printf (b, "<tr><td>%s</td><td><input type='text'"
            "name='%s' value='%s' size='%d'/></td></tr>\n",
	    name, path, ch, tag->width);
	  break;
      }
    }				/* end for each repeated field	*/
    if (tag->type == TAG_FIELD)	/* button to add field		*/
    {
      dbuf_printf (b, "<tr><td><button type='button' "
	"onClick='addfield(this)'>"
        "Add Field</button></td><td></td></tr>");
    }
    tag++;
  }				/* end for each tag		*/
  dbuf_printf (b, "</table>\n");
  return (0);
}

/*
 * add tabs for all repeated prefixes
 */
int config_getTabs (XML *xml, int t, DBUF *b)
{
  int l, i, n;
  char *prefix,
       *ch,
       path[MAX_PATH];
  CTAG *tags;

  debug ("getting prefix and tags...\n");
  prefix = Tabs[t].prefix;
  tags = Tabs[t].tags;
  if (!Tabs[t].repeats)
  {
    debug ("non-repeated tags...\n");
    return (config_getRows (xml, prefix, tags, 0, b));
  }
  n = xml_count (xml, prefix);
  // start a new tab
  dbuf_printf (b, "<div id='cfg%d' class='configlayout' >\n"
    "<ul id='cfg%d-nav' class='configlayout' >\n", t, t);
  // add the tabs
  for (i = 0; i < n; i++)
  {
    sprintf (path, "%s[%d].%s", prefix, i, tags->tag);
    ch = xml_get_text (xml, path);
    if (*ch == 0)
      ch = "(not named)";
    dbuf_printf (b, "<li><a href='#tag%d%d'>%s</a></li>\n", t, i + 1, ch);
  }
  dbuf_printf (b, "<li><a href='#tag%d%d'>NEW</a></li>\n"
    "</ul><div class='tabs-container'>",
    t, i + 1);
  // add the form items
  for (i = 0; i < n; i++)
  {
    sprintf (path, "%s[%d].%s", prefix, i, tags->tag);
    ch = xml_get_text (xml, path);
    if (*ch == 0)
      ch = "(not named)";
    dbuf_printf (b, "<div class='tab%d' id='tag%d%d'>\n", t, t, i + 1);
    sprintf (path, "%s[%d]", prefix, i);
    config_getRows (xml, path, tags, 0, b);
    dbuf_printf (b, "</div>\n");
  }
  sprintf (path, "%s[%d]", prefix, i);
  dbuf_printf (b, "<div class='tab%d' id='tag%d%d'>\n", t, t, i + 1);
  config_getRows (xml, path, tags, 1, b);
  dbuf_printf (b, "</div>\n");
  dbuf_printf (b, "</div></div>\n"
    "<script type='text/javascript'>\n"
    "var tabber%d = new Yetii({ id: 'cfg%d', tabclass: 'tab%d', "
    "persist: true });\n</script>\n", t, t, t);
  return (0);
}

/*
 * add select and submit buttons for CPA generation
 */
int config_selectCpa (XML *xml, DBUF *b)
{
  int i, n, l;
  char *ch, path[MAX_PATH];

  l = sprintf (path,  "Phineas.Sender.RouteInfo.Route");
  n = xml_count (xml, path);
  if (n < 1)
    return (0);
  dbuf_printf (b, "<h3>CPA Routes</h3>");
  for (i = 0; i < n; i++)
  {
    sprintf (path + l, "[%d].Name", i);
    ch = xml_get_text (xml, path);
    if (*ch == 0)
      continue;
    dbuf_printf (b,
      "<input type='radio' name='cpa' value='%d' /> %s<br>\n", i, ch);
  }
  dbuf_printf (b, "<br><br>"
      "<input type='submit' name='submit' value='Export CPA Route' />\n");
  return (0);
}

/*
 * build the input
 */
DBUF *config_getConfig ()
{
  XML *xml;
  DBUF *b;
  int i;

  debug ("current configuraton is %s\n", EditConfig);
  xml = NULL;
  if ((*EditConfig == 0) || ((xml = xml_load (EditConfig)) == NULL))
    strcpy (EditConfig, ConfigName);
  if ((xml == NULL) && ((xml = xml_load (EditConfig)) == NULL))
  {
    error ("Can't load configuration!\n");
    return (NULL);
  }
  debug ("loaded configuraton %s\n", EditConfig);

  b = dbuf_alloc ();
#ifdef UNITTEST
  dbuf_printf (b, "<html><head>"
    "<title>Test Configuration</title>\n"
    "<link rel='stylesheet' type='text/css' href='console.css' />\n"
    "<SCRIPT src='yetii.js' type=text/javascript></SCRIPT>\n"
    "<SCRIPT src='config.js' type=text/javascript></SCRIPT>\n"
    "</head><body><h2>Configuration Edit Test</h2>\n"
    "<div id='console'>\n");
#endif
  debug ("creating navigation...\n");
  dbuf_printf (b, "<div id='configform' class='configlayout' >\n"
    "<ul id='configform-nav' class='configlayout' >\n");
  for (i = 0; Tabs[i].name != NULL; i++)
  {
    dbuf_printf (b, "<li><a href='#tab%d'>%s</a></li>\n",
      i + 1, Tabs[i].name);
  }
  dbuf_printf (b, "<li><a href='#tab%d'>Load/Save/Export</a></li>\n"
    "</ul><div class='tabs-container'>\n"
    "<form method='POST' action='#' >\n",
      i + 1);
  debug ("adding tabs...\n");
  for (i = 0; Tabs[i].name != NULL; i++)
  {
    dbuf_printf (b, "<div class='tab' id='tab%d'>\n");
    config_getTabs (xml, i, b);
    dbuf_printf (b, "</div>\n");
  }
  debug ("adding save/load tab...\n");
  dbuf_printf (b, "<div class='tab' id='tab%d'>\n"
      "<h3>Load/Save/Export</h3>\nConfiguration File Name "
      "<input type='text' name='EditConfig' value='%s' size='%d'/>\n",
      i + 1, EditConfig, FILE_WIDTH);
  config_selectCpa (xml, b);
  dbuf_printf (b,
    "<input type='submit' name='submit' value='Save Configuration' />\n"
    "<input type='submit' name='submit' value='Load Configuration' />\n"
    "</div>\n</form></div></div>\n<script type='text/javascript'>\n"
    "var tabber = new Yetii({ id: 'configform', "
    "persist: true });\n</script>\n");
#ifdef UNITTEST
  dbuf_printf (b, "</div></body></html>");
#endif
  debug ("freeing temp xml...\n");
  xml_free (xml);
  return (b);
}

/*
 * get the edit configuration load/store filename and return
 * 0 = load, 1 = save, 2 = export, -1 = error
 */
int config_getEditConfig (char *data)
{
  int action;
  char *ch, *amp, path[MAX_PATH];

  /* first determine what action we are taking */
  if ((ch = strstr (data, "&submit=")) == NULL)
  {
    error ("POST missing submit parameter\n");
    return (-1);
  }
  ch += 8;
  debug ("%.4s EditConfig %s\n", ch, EditConfig);
  if (strncmp (ch, "Save+", 5) == 0)
    action = 1;
  else if (strncmp (ch, "Load+", 5) == 0)
    action = 0;
  else if (strncmp (ch, "Export+", 7) == 0)
    return (2);
  else
  {
    error ("Invalid submit action %.5s", ch);
    return (-1);
  }
  if ((ch = strstr (data, "&EditConfig=")) == NULL)
  {
    error ("POST missing EditConfig paramter\n");
    return (-1);
  }
  ch += 12;
  if ((amp = strchr (ch, '&')) == NULL)
  {
    error ("EditConfig paramter not terminated\n");
    return (-1);
  }
  strncpy (path, ch, amp - ch);
  path [amp - ch] = 0;
  if (*path == 0)
  {
    error ("Empty EditConfig parameter\n");
    return (-1);
  }
  pathf (EditConfig, "%s", urldecode (path));
  return (action);
}

/*
 * process the post request
 */
DBUF *config_setConfig (XML *xml, char *req)
{
  XML *cfg;
  int l, action, route = -1;
  char *r, *eq, *amp;
  char skip[MAX_PATH],
       path[MAX_PATH],
       value[128];
  DBUF *console_doGet ();

  /*
   * skip past headers
   */
  if ((r = req) == NULL)
  {
    error ("NULL console POST request\n");
    return (NULL);
  }
  debug ("POST skipping headers...\n");
  while (*r)
  {
    if (*r++ != '\n')
      continue;
    if (*r == '\r')
      r++;
    if (*r == '\n')
      break;
  }
  if (*r++ != '\n')
  {
    error ("Malformed request %s\n", req);
    return (NULL);
  }
  debug ("getting config name and submit action...\n");
  if ((action = config_getEditConfig (r)) < 1)
    return (console_doGet (xml, req));

  debug ("building new configuration...\n");
  cfg = xml_alloc ();
  strcpy (skip, "NONE");
  while (1)
  {
    if ((eq = strchr (r, '=')) == NULL)
      break;
    strncpy (path, r, l = eq++ -r);
    path[l] = 0;
    if ((amp = strchr (r, '&')) == NULL)
    {
      strcpy (value, eq);
    }
    else
    {
      strncpy (value, eq, amp - eq);
      value[amp - eq] = 0;
    }
    urldecode (path);
    if ((l = strlen (path) - 6) < 0)
      l = 0;
    if ((*value == 0) && (strcmp (path + l, "].Name") == 0))
    {
      strcpy (skip, path);
      skip[l+2] = 0;
      debug ("skip set to '%s'\n", skip);
    }
    if (strncmp (path, "Phineas.", 8) == 0)
    {
      if (strncmp (skip, path, strlen (skip)))
        xml_set_text (cfg, urldecode (path), urldecode (value));
    }
    else if ((strcmp (path, "cpa") == 0) && isdigit (*value))
      route = atoi (value);
    if (amp == NULL)
      break;
    r = amp + 1;
  }
  if (action == 2)			/* export Cpa		*/
  {
    debug ("export route %d\n", route);
    if (route >= 0)
      cpa_create (cfg, route);
  }
  else
  {
    debug ("format and save to %s\n", EditConfig);
    xml_beautify (cfg, 2);
    if (backup (EditConfig))
      error ("Can't backup %s - %s\n", EditConfig, strerror (errno));
    else
    xml_save (cfg, EditConfig);
  }
  xml_free (cfg);
  return (console_doGet (xml, req));
}

#ifdef UNITTEST

DBUF *console_doGet (XML *xml, char *req)
{
  DBUF *b = dbuf_alloc ();
  dbuf_printf (b, "The response\n");
  return (b);
}

int main (int argc, char **argv)
{
  DBUF *b;
  XML *xml;

  if ((xml = xml_parse (PhineasConfig)) == NULL)
    fatal ("Failed parsing PhineasConfig\n");
  loadpath (xml_get_text (xml, "Phineas.InstallDirectory"));
  xml_free (xml);
  pathf (ConfigName, "bin/Phineas.xml");
  b = config_getConfig ();
  writefile ("../console/test.html", dbuf_getbuf (b), dbuf_size (b));
  // debug ("%.*s\n", dbuf_size (b), dbuf_getbuf (b));
  dbuf_free (b);
  info ("%s unit test completed\n", argv[0]);
}

#endif
