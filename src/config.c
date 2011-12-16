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

#ifdef UNITTEST
#define __CONSOLE__
#endif

#ifdef __CONSOLE__

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
 * these tag names are magic
 */
char REQUEST[] = "ConfigurationRequest",
     CONFIG[] = "Configuration.File.Name",
     EXPORT[] = "Export.CPA",
     SAVE[] = "Save.Configuration",
     LOAD[] = "Load.Configuration",
     UPDATE[] = "Update.Configuration";

/*
 * current configuration in edit
 */
char EditConfig[MAX_PATH] = "";
XML *EditXml = NULL;
XML *ConfigXml = NULL;

/*
 * default tag widths
 */
#define FILE_WIDTH 50
#define PORT_WIDTH 4
#define DIR_WIDTH 50
#define PASS_WIDTH 14
#define NUM_WIDTH 4

/*
 * tag types
 */
#define TAG_TEXT 0
#define TAG_DIR 1
#define TAG_FILE 2
#define TAG_SELECT 3
#define TAG_RADIO 4
#define TAG_PASSWD 5
#define TAG_NUMBER 6
#define TAG_SUBMIT 7

// most options for a list
#define MAXOPT 50

/*
 * reset configuration globals
 */
void config_reset ()
{
  *EditConfig = 0;
  if (EditXml != NULL)
    EditXml = xml_free (EditXml);
  if (ConfigXml != NULL)
    ConfigXml = xml_free (ConfigXml);
}

int config_inputtype (char *pre, int input)
{
  char *ch;

  ch = xml_getf (ConfigXml, "%s.Input[%d].Type", pre, input);
  // debug ("%s.Input[%d].Type=%s\n", pre, input, ch);
  if (strcmp (ch, "text") == 0)
    return (TAG_TEXT);
  if (strcmp (ch, "dir") == 0)
    return (TAG_DIR);
  if (strcmp (ch, "file") == 0)
    return (TAG_FILE);
  if (strcmp (ch, "select") == 0)
    return (TAG_SELECT);
  if (strcmp (ch, "password") == 0)
    return (TAG_PASSWD);
  if (strcmp (ch, "number") == 0)
    return (TAG_NUMBER);
  if (strcmp (ch, "radio") == 0)
    return (TAG_RADIO);
  if (strcmp (ch, "submit") == 0)
    return (TAG_SUBMIT);
  return (TAG_TEXT);
}

/*
 * issue javascript help attributes
 */

int config_addhelp (char *help, DBUF *b)
{
  int prevc;
  char *ch;

  if (strempty (ch = help))
    return (0);
  dbuf_printf (b, " OnMouseOver=\"showHelp (this, '");
  prevc = ' ';
  while (*ch)
  {
    if (isspace (*ch))
    {
      if (!isspace (prevc))
        dbuf_putc (b, ' ');
    }
    else if (*ch == '"')
      dbuf_printf (b, "&quot;");
    else if (*ch == '\'')
      dbuf_printf (b, "\\'");
    else
      dbuf_putc (b, *ch);
    prevc = *ch++;
  }
  return (dbuf_printf (b, "')\" onMouseOut=\"hideHelp()\" "));
}

int config_help (char *pre, int input, DBUF *b)
{
  char *ch;

  if ((ch = xml_getf (ConfigXml, "%s.Input[%d].Help", pre, input)) == NULL)
    return (0);
  return (config_addhelp (ch, b));
}

int config_tabhelp (char *pre, int tab, DBUF *b)
{
  char *ch;

  if ((ch = xml_getf (ConfigXml, "%s[%d].Help", pre, tab)) == NULL)
    return (0);
  return (config_addhelp (ch, b));
}

/*
 * construct a select input from a list of values
 */

int config_select (char *name, char *val, char **list, DBUF *b)
{
  char *ch;

  dbuf_printf (b, "<select size='1' name='%s'>\n", name);
  while (*list != NULL)
  {
    dbuf_printf (b, "<option %svalue='%s'>%s</option>\n",
      strcmp (val, *list) ? "" : "selected ", *list,
      **list ? *list : "(none selected)");
    list++;
  }
  dbuf_printf (b, "</select>\n");
  return (0);
}

/*
 * construct a radio input from a list of values
 */
int config_radio (char *name, char *val, char **list, DBUF *b)
{
  int i;

  for (i = 0; list[i] != NULL; i++)
  {
    dbuf_printf (b, 
      "<input type='radio' name='%s' value='%s' %s/> %s<br>\n",
      name, list[i], strcmp (val, list[i]) ? "" : "checked", list[i]);
  }
  return (0);
}

/*
 * create a list of option or references for a tag
 */
int config_list (XML *xml, char *configpre, int tag, char **list)
{
  char *ch,
       *ref,
       path[MAX_PATH];
  int i, n, nopt;

  n = 0;
  ref = xml_getf (ConfigXml, "%s.Input[%d].Ref", configpre, tag);
  if (*ref)
  {
    nopt = xml_count (xml, ref);
    for (i = 0; i < nopt; i++)
    {
      ch = xml_getf (xml, "%s[%d].Name", ref, i);
      if (*ch && (n < MAXOPT))
	list[n++] = ch;
    }
  }
  else
  {
    sprintf (path, "%s.Input[%d].Option", configpre, tag);
    nopt = xml_count (ConfigXml, path);
    for (i = 0; i < nopt; i++)
    {
      ch = xml_getf (ConfigXml, "%s.Input[%d].Option[%d]", 
	configpre, tag, i);
      if (n < MAXOPT)
	list[n++] = ch;
    }
  }
  list[n] = NULL;
  return (n);
}

char *config_prompt (char *dst, char *src)
{
  char *p;

  p = dst;
  while (*src)
  {
    if ((*p = *src++) == '.')
      *p = ' ';
    p++;
  }
  *p = 0;
  return (dst);
}


/*
 * build a form input for this tag
 */
int config_getInput (XML *xml, char *configpre, int input, 
  char *path, char *name, DBUF *b)
{
  int sz, width;
  char *value, 
       prompt[MAX_PATH],
       *list[MAXOPT];

  // get the default value
  if (strstarts (path, "Phineas."))
    value = xml_get_text (xml, path);
  else if (strcmp (name, CONFIG) == 0)
    value = EditConfig;
  else
    value = "";
  config_prompt (prompt, name);
  if (*path == 0)
    path = name;
  // debug ("input path=%s prompt=%s value=%s\n", path, prompt, value);

  sz = dbuf_size (b);
  dbuf_printf (b, "<tr><td valign='top'>%s</td><td", prompt);
  config_help (configpre, input, b);
  dbuf_printf (b, ">");
  width = 24;
  switch (config_inputtype (configpre, input))
  {
    case TAG_SUBMIT:
      dbuf_setsize (b, sz);
      dbuf_printf (b, "<tr><td colspan='2'");
      config_help (configpre, input, b);
      dbuf_printf (b, "><button type='button' name='%s' value='%s'"
	"onClick='setRequest (this, \"%s\")'>%s</button>",
        name, prompt, path, prompt);
      break;
    case TAG_SELECT :
      if (config_list (xml, configpre, input, list))
        config_select (path, value, list, b);
      break;
    case TAG_RADIO :
      if (config_list (xml, configpre, input, list))
        config_radio (path, value, list, b);
      break;
    case TAG_PASSWD :
      dbuf_printf (b, "<input type='password' name='%s' value='%s' "
        "size='%d'/>", path, value, PASS_WIDTH);
      break;
    case TAG_FILE :
      width = FILE_WIDTH;
      goto istext;
    case TAG_DIR :
      width = DIR_WIDTH;
      goto istext;
    case TAG_NUMBER :
      width = NUM_WIDTH;
      goto istext;
    case TAG_TEXT :
      width = atoi (xml_getf (ConfigXml, "%s.Input[%d].Width", 
	configpre, input));
      goto istext;
    default :
istext:
      dbuf_printf (b, "<input type='text' name='%s' value='%s' "
        "size='%d'/>", path, value, width);
      break;
  }
  dbuf_printf (b, "</td></tr>\n");
  return (0);
}

/*
 * get inputs for a repeated field
 */
int config_getRepeats (XML *xml, char *configpre, int input,
  char *path, char *name, DBUF *b)
{
  int i,
      repeats,
      path_len,
      name_len;
  char hbuf[124];

  if ((repeats = xml_count (xml, path)) == 0)
    repeats = 1;
  path_len = strlen (path);
  name_len = strlen (name);
  for (i = 0; i < repeats; i++)
  {
    if (path_len)
      sprintf (path + path_len, "[%d]", i);
    sprintf (name + name_len, " %d", i + 1);
    config_getInput (xml, configpre, input, path, name, b);
  }
  name[name_len] = 0;
  path[path_len] = 0;
  sprintf (hbuf, "Click this to add another %s to the end of the list", 
    name);
  dbuf_printf (b, "<tr><td");
  config_addhelp (hbuf, b);
  dbuf_printf (b, "><button type='button' "
    "name='%s' onClick='addfield(this)'>"
    "Add %s</button></td><td></td></tr>", name, name);
  return (0);
}

/*
 * add a table of inputs for a (possibly repeated) tab
 */
int config_getTable (XML *xml, char *configpre, int index, DBUF *b)
{
  int t, num_inputs;
  char *suffix,
       *prefix,
       name[MAX_PATH],
       path[MAX_PATH];

  prefix = xml_getf (ConfigXml, "%s.Prefix", configpre);
  sprintf (path, "%s.Input", configpre);
  num_inputs = xml_count (ConfigXml, path);
  debug ("adding %d inputs for %s prefix=%s\n", num_inputs, path, prefix);
  dbuf_printf (b, "<table border=0 summary='%s'>\n", prefix);
  for (t = 0; t < num_inputs; t++)
  {
    suffix = xml_getf (ConfigXml, "%s.Input[%d].Suffix", configpre, t);
    if (*prefix && *suffix)
    {
      if (index)
        sprintf (path, "%s[%d].%s", prefix, index, suffix);
      else
	sprintf (path, "%s.%s", prefix, suffix);
    }
    else
    {
      *path = 0;
    }
    strcpy (name, xml_getf (ConfigXml, "%s.Input[%d].Name", configpre, t));
    if (*name == 0)
      strcpy (name, suffix);
    // debug ("path=%s name=%s\n", path, name);
    if (!strcmp (xml_getf (ConfigXml, "%s.Input[%d].Repeats", 
      configpre, t), "true"))
    {
      config_getRepeats (xml, configpre, t, path, name, b);
    }
    else
    {
      config_getInput (xml, configpre, t, path, name, b);
    }
  }				/* end for each input tag	*/
  dbuf_printf (b, "</table>\n");
  return (0);
}

/*
 * build a unique id suffix for a configuration prefix
 */
char *config_id (char *id, char *configpre)
{
  char *ch, *p;

  p = id;
  ch = configpre;
  while ((ch = strchr (ch, '[')) != NULL)
  {
    *p++ = '_';
    while (*++ch != ']')
      *p++ = *ch;
  }
  *p = 0;
  return (id);
}

/*
 * add tab contents.  Creates subtabs when repeats 
 */
int config_getContents (XML *xml, char *configpre, DBUF *b)
{
  int l, i, n;
  char *ch, *prefix,
       id[20],
       path[MAX_PATH];

  debug ("getting prefix and tags...\n");
  if (strcmp (xml_getf (ConfigXml, "%s.Repeats", configpre), "true"))
  {
    debug ("non-repeated tags...\n");
    return (config_getTable (xml, configpre, 0, b));
  }
  config_id (id, configpre);
  // get the configuration data prefix
  prefix = xml_getf (ConfigXml, "%s.Prefix", configpre);
  // and count number of repeated entries
  n = xml_count (xml, prefix);
  // start a new tab list
  dbuf_printf (b, "<div id='configform%s' class='configlayout' >\n"
    "<ul id='configform%s-nav' class='configlayout' >\n", id, id);
  // add the tabs using the first tag for this prefix
  for (i = 0; i < n; i++)
  {
    ch = xml_getf (xml, "%s[%d].%s", prefix, i, 
      xml_getf (ConfigXml, "%s.Input[0].Suffix", configpre));
    if (*ch == 0)
      ch = "(not named)";
    dbuf_printf (b, "<li><a href='#tag%s_%d'>%s</a></li>\n", id, i + 1, ch);
  }
  dbuf_printf (b, "<li><a href='#tag%s_%d'>NEW</a></li>\n"
    "</ul><div class='tabs-container'>",
    id, i + 1);
  // add the form items
  for (i = 0; i <= n; i++)
  {
    dbuf_printf (b, "<div class='tab%s' id='tag%s_%d'>\n", id, id, i + 1);
    config_getTable (xml, configpre, i, b);
    dbuf_printf (b, "</div>\n");
  }
  dbuf_printf (b, "</div></div>\n"
    "<script type='text/javascript'>\n"
    "var tabber%s = new Yetii({ id: 'configform%s', tabclass: 'tab%s', "
    "persist: true });\n</script>\n", id, id, id);
  return (0);
}

/*
 * get all the tabs for current config prefix
 */
config_getTabs (XML *xml, char *configpre, DBUF *b)
{
  int i, n;
  char id[20], prefix[MAX_PATH];

  config_id (id, configpre);

  debug ("creating navigation...\n");

  dbuf_printf (b, "<div id='configform%s' class='configlayout' >\n"
    "<ul id='configform%s-nav' class='configlayout' >\n", id, id);
  sprintf (prefix, "%s.Tab", configpre);
  n = xml_count (ConfigXml, prefix);
  for (i = 0; i < n; i++)
  {
    dbuf_printf (b, "<li");
    config_tabhelp (prefix, i, b);
    dbuf_printf (b, "><a href='#tab%s_%d'>%s</a></li>\n",
      id, i + 1, xml_getf (ConfigXml, "%s[%d].Name", prefix, i));
  }
  dbuf_printf (b, "</ul><div class='tabs-container'>\n");

  debug ("adding tabs...\n");
  for (i = 0; i < n; i++)
  {
    dbuf_printf (b, "<div class='tab' id='tab%s_%d'>\n", id, i);
    sprintf (prefix, "%s.Tab[%d]", configpre, i);
    config_getContents (xml, prefix, b);
    dbuf_printf (b, "</div>\n");
  }
  dbuf_printf (b,
    "</div></div>\n<script type='text/javascript'>\n"
    "var tabber = new Yetii({ id: 'configform%s', "
    "persist: true });\n</script>\n", id);
}

/*
 * remove unwanted tags from the configuration
 */
int config_detag (char *path, char *pathp, char *prefix)
{
  int i, n, pl;
  char *ch;
  char p[MAX_PATH];


  pl = strlen (path);
  strcpy (path + pl, ".Input");
  n = xml_count (ConfigXml, path);
  debug ("checking %d %s for %s\n", n, path, prefix);
  i = 0;
  while (i < n)
  {
    sprintf (path + pl, ".Input[%d]", i);
    sprintf (p, "%s.%s", pathp, xml_getf (ConfigXml, "%s.Suffix", path));
    ch = xml_getf (ConfigXml, "%s.Ref", path);
    if (strstarts (p, prefix) || strstarts (ch, prefix))
    {
      debug ("deleting config %s\n", path);
      xml_delete (ConfigXml, path);
      n--;
    }
    else
    {
      i++;
    }
  }
  path[pl] = 0;
  return (0);
}

/*
 * remove unwanted tabs from the configuration
 */
int config_detab (char *path, char *prefix)
{
  int i, n, pl;
  char *ch;

  pl = strlen (path);
  strcpy (path + pl, ".Tab");
  n = xml_count (ConfigXml, path);
  debug ("checking %d %s for %s\n", n, path, prefix);
  i = 0;
  while (i < n)
  {
    sprintf (path + pl, ".Tab[%d]", i);
    ch = xml_getf (ConfigXml, "%s.Prefix", path);
    if (strstarts (ch, prefix))
    {
      debug ("deleting config %s\n", path);
      xml_delete (ConfigXml, path);
      n--;
    }
    else
    {
      if (strstarts (prefix, ch))
	config_detag (path, ch, prefix);
      i++;
    }
  }
  path[pl] = 0;
  return (0);
}

/*
 * set up for configuration entry
 */
int config_setup ()
{
  int i, n;
  char path[MAX_PATH];

  if (*EditConfig == 0)
    strcpy (EditConfig, ConfigName);
  debug ("current configuraton is %s\n", EditConfig);
  if ((EditXml != NULL) && (ConfigXml != NULL))
    return (0);
  ConfigXml = xml_free (ConfigXml);
  if ((EditXml = xml_load (EditConfig)) == NULL)
  {
    error ("Can't load %s\n", EditConfig);
    return (-1);
  }
  pathf (path, "%s", xml_get_text (EditXml, "Phineas.Console.Config"));
  debug ("getting console configuration %s\n", path);
  if ((ConfigXml = xml_load (path)) == NULL)
  {
    error ("Can't load %s\n", path);
    return (-1);
  }
  /* conditionally remove chunks we don't use			*/
  strcpy (path, "Config");
#ifndef __SENDER__
  config_detab (path, "Phineas.Sender");
#endif
#ifndef __RECEIVER__
  config_detab (path, "Phineas.Receiver");
#endif
#ifndef __SERVER__
  config_detab (path, "Phineas.Server");
#endif
  debug ("loaded configuraton %s\n", EditConfig);
  return (0);
}

/*
 * build the input
 */
DBUF *config_getConfig ()
{
  DBUF *b;
  int i, n;
  char prefix[MAX_PATH];

  if (config_setup ())
    return (NULL);

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
  dbuf_printf (b, "<form method='POST' action='#' >\n"
    "<input type='hidden' name='%s' id='%s' />", REQUEST, REQUEST);
  config_getTabs (EditXml, "Config", b);
  dbuf_printf (b, "<div style=\"height: 100px\"></form>\n");
#ifdef UNITTEST
  dbuf_printf (b, "</div></body></html>");
#endif
  return (b);
}

/***************** POST processing *********************************/

/*
 * get the filename value and return submit type...
 * 0 = load, 1 = save, 2 = export, 3 = update -1 = error
 */
int config_getPosted (char *data)
{
  int action;
  char *ch, 
       *amp, 
       path[MAX_PATH];
  char *submit[] =
  {
    LOAD, SAVE, EXPORT, UPDATE
  };

  if ((ch = strstr (data, REQUEST)) == NULL)
  {
    error ("Missing %s parameter from POST\n", REQUEST);
    return (-1);
  }
  ch += strlen (REQUEST) + 1;
  debug ("request value=%.20s\n", ch);
  /* first determine what action we are taking */
  for (action = 3; action >= 0; action--)
  {
    if (strstarts (ch, submit[action]))
      break;
  }
  if (action < 0)
  {
    error ("POST missing request value\n");
    return (-1);
  }
  sprintf (path, "%s=", CONFIG);
  if ((ch = strstr (data, path)) == NULL)
  {
    error ("POST missing configuration file name\n");
    return (-1);
  }
  ch += strlen (CONFIG) + 1;
  if ((amp = strchr (ch, '&')) == NULL)
  {
    strcpy (path, ch);
  }
  else
  {
    strncpy (path, ch, amp - ch);
    path [amp - ch] = 0;
  }
  if (*path == 0)
  {
    error ("Empty %s parameter\n", CONFIG);
    return (-1);
  }
  pathf (EditConfig, "%s", urldecode (path));
  debug ("action=%d EditConfig=%s\n", action, EditConfig);
  return (action);
}

/*
 * a simply struct to keep track of tags
 */
typedef struct configtag
{
  struct configtag *next;
  char tag[1];
} CONFIGTAG;

CONFIGTAG *config_addtag (CONFIGTAG *list, char *tag)
{
  CONFIGTAG *t;

  t = (CONFIGTAG *) malloc (sizeof (CONFIGTAG) + strlen (tag));
  strcpy (t->tag, tag);
  t->next = list;
  return (t);
}

void config_deltags (CONFIGTAG *list)
{
  CONFIGTAG *t;
  while (list != NULL)
  {
    t = list;
    list = t->next;
    debug ("deleting tag %s\n", t->tag);
    xml_delete (EditXml, t->tag);
    free (t);
  }
}

/*
 * process the post request
 */
DBUF *config_setConfig (XML *xml, char *req)
{
  int l, action;
  char *r, *eq, *amp;
  CONFIGTAG *dtags = NULL;
  char route[80],
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
  /*
   * determine what action to take
   */
  debug ("getting config name and submit action...\n");
  if ((action = config_getPosted (r)) < 1)
  {
    if (action == 0)			/* Load...		*/
      EditXml = xml_free (EditXml);
    return (console_doGet (xml, req));
  }

  debug ("updating configuration...\n");
  if (EditXml == NULL)
    EditXml = xml_alloc ();
  *route = 0;				/* no route seen yet	*/
  while (1)
  {
    if ((eq = strchr (r, '=')) == NULL)	/* no more post args	*/
      break;
    strncpy (path, r, l = eq++ -r);	/* note the (path) name	*/
    path[l] = 0;
    if ((amp = strchr (r, '&')) == NULL)
    {					/* get the value	*/
      strcpy (value, eq);
    }
    else
    {
      strncpy (value, eq, amp - eq);
      value[amp - eq] = 0;
    }
    urldecode (path);
    /*
     * If the value is empty and this is a name, or a repeated field
     * then note it for later deletion.
     */
    if (*value == 0)
    { 
      l = strlen (path);
      if (strcmp (path + l - 6, "].Name") == 0)
      {
	path[l - 5] = 0;
	dtags = config_addtag (dtags, path);
	path[l - 5] = 'N';
      }
      else if (path[l -1] == ']')
	dtags = config_addtag (dtags, path);
    }
    /*
     * if the path is into our configuration
     * then set the new value
     */
    if (strstarts (path, "Phineas."))
    {
      xml_set_text (EditXml, path, urldecode (value));
    }
    else 
    {
      if ((strcmp (path, REQUEST) == 0) && *value)
      {
	urldecode (value);
	if (strstarts (value, EXPORT))	/* note export route	*/
	{
          strcpy (route, strchr (value, ':') + 1);
	  debug ("route reference=%s\n", route);
	}
      }
    }
    if (amp == NULL)			/* next post arg	*/
      break;
    r = amp + 1;
  }
  config_deltags (dtags);		/* delete empty tags	*/
  if (action == 1)			/* save configuration	*/
  {
    debug ("format and save to %s\n", EditConfig);
    xml_beautify (EditXml, 2);
    if (backup (EditConfig))
      error ("Can't backup %s - %s\n", EditConfig, strerror (errno));
    else
      xml_save (EditXml, EditConfig);
  }
  else if (action == 2) 		/* export Cpa		*/
  {
    r = xml_get_text (EditXml, route);
    debug ("export route for %s at %s\n", r, route);
    cpa_create (EditXml, cpa_route (EditXml, r));
  }
  					/* just update		*/
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

#endif /* UNITTEST */
#endif /* __CONSOLE__ */
